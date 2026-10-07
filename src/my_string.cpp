// ============================================================================
// 作业 2：String 类的实现文件
//
// 目前本文件是空的：构建时链接阶段会报 "undefined reference to `String::...`"，
// 这是预期现象。请先到 include/my_string.h 中补好私有数据成员，再在这里实现
// 所有声明过的成员函数与运算符。
//
// 如果你想拆成多个 .cpp 文件，请同步修改根目录 CMakeLists.txt 中的
// STRING_SOURCES 列表。
//
// 实现清单（与 include/my_string.h 一一对应）：
//   [ ] String() / String(const char*) / 拷贝构造 / 移动构造 / 析构
//   [ ] 复制赋值 operator=(const String&) / 移动赋值 operator=(String&&)
//   [ ] operator+ / operator[]（含 const 版本）/ at（含 const 版本）
//   [ ] size / capacity
//   [ ] insert / push_back
//   [ ] c_str / operator const char*
//   [ ] swap
//   [ ] friend operator<< / operator>>
//
// 完成后按 docs/build-and-test.md 的步骤构建、运行测试并做 ASan/UBSan 检查。
// ============================================================================

#include "my_string.h"
#include <cctype>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace {

// 默认构造的空串容量契约：capacity() >= 16（不含结尾 '\0'）
constexpr std::size_t kDefaultCapacity = 16;

}  // namespace

// ============================================================================
// 构造 / 析构（Rule of Five）
// ============================================================================

String::String() : data_(new char[kDefaultCapacity + 1]), size_(0),
                   capacity_(kDefaultCapacity) {
    data_[0] = '\0';
}

String::String(const char* str) {
    if (str == nullptr) {
        str = "";
    }
    std::size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    data_ = new char[len + 1];
    size_ = len;
    capacity_ = len;
    for (std::size_t i = 0; i <= len; ++i) {
        data_[i] = str[i];  // 连结尾 '\0' 一起复制
    }
}

String::String(const String& other)
    : data_(new char[other.size_ + 1]), size_(other.size_),
      capacity_(other.size_) {
    for (std::size_t i = 0; i <= size_; ++i) {
        data_[i] = other.data_[i];
    }
}

String::String(String&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    // 窃取缓冲区后，把源对象置为“有效但内容未指定”的空状态
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

String::~String() {
    delete[] data_;  // delete[] nullptr 是合法的（被移动后的对象）
}

// ============================================================================
// 赋值
// ============================================================================

String& String::operator=(const String& other) {
    if (this != &other) {
        // copy-and-swap：先深拷贝（失败时 *this 不变），成功后再交换
        String tmp(other);
        swap(tmp);
    }
    return *this;
}

String& String::operator=(String&& other) noexcept {
    if (this != &other) {  // 自移动赋值直接跳过，保证对象仍然有效
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

// ============================================================================
// 拼接
// ============================================================================

String String::operator+(const String& other) const {
    String result;  // 这里有一次分配：分配失败时抛出，两个操作数不受影响
    const std::size_t total = size_ + other.size_;
    if (total > result.capacity_) {
        result.grow_to(total);
    }
    for (std::size_t i = 0; i < size_; ++i) {
        result.data_[i] = data_[i];
    }
    for (std::size_t i = 0; i < other.size_; ++i) {
        result.data_[size_ + i] = other.data_[i];
    }
    result.size_ = total;
    result.data_[total] = '\0';
    return result;
}

// ============================================================================
// 下标访问（不做边界检查；s[size()] 返回结尾 '\0' 的引用）
// ============================================================================

char& String::operator[](std::size_t index) noexcept {
    return data_[index];
}

const char& String::operator[](std::size_t index) const noexcept {
    return data_[index];
}

// ============================================================================
// 带边界检查的访问
// ============================================================================

char& String::at(std::size_t index) {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}

const char& String::at(std::size_t index) const {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}

// ============================================================================
// 长度 / 容量
// ============================================================================

std::size_t String::size() const noexcept {
    return size_;
}

std::size_t String::capacity() const noexcept {
    return capacity_;
}

// ============================================================================
// 插入 / 追加
// ============================================================================

void String::insert(std::size_t pos, const String& str) {
    if (pos > size_) {
        throw std::out_of_range("String::insert: position out of range");
    }
    const std::size_t add = str.size_;
    if (add == 0) {
        return;  // 插入空串：什么都不用做
    }
    const std::size_t new_size = size_ + add;

    if (new_size <= capacity_) {
        // ---- 原地插入（不重新分配）----
        if (&str == this) {
            // 自插入：先把原尾部 [pos, size_) 右移到 [pos+size_, 2*size_)，
            // 再从后往前把整段 [0, size_) 复制到 [pos, pos+size_)。
            // 两个区域互不重叠，且倒序复制保证源字符在覆盖前已被读取。
            for (std::size_t i = size_; i > pos; --i) {
                data_[i + size_ - 1] = data_[i - 1];
            }
            std::size_t i = size_;
            while (i != 0) {
                --i;
                data_[pos + i] = data_[i];
            }
        } else {
            // 普通插入：先把 [pos, size_) 右移 add 个位置腾出空间
            for (std::size_t i = size_; i > pos; --i) {
                data_[i + add - 1] = data_[i - 1];
            }
            for (std::size_t i = 0; i < add; ++i) {
                data_[pos + i] = str.data_[i];
            }
        }
        size_ = new_size;
        data_[size_] = '\0';
    } else {
        // ---- 需要扩容：先分配新缓冲区（失败则对象不变）----
        char* nb = new char[new_size + 1];
        for (std::size_t i = 0; i < pos; ++i) {
            nb[i] = data_[i];
        }
        // 自插入时 str 与 *this 是同一对象：旧缓冲区在 delete 前一直存活，
        // 所以这里读 str.data_（旧缓冲区）不会与写 nb 冲突。
        for (std::size_t i = 0; i < add; ++i) {
            nb[pos + i] = str.data_[i];
        }
        for (std::size_t i = 0; i < size_ - pos; ++i) {
            nb[pos + add + i] = data_[pos + i];
        }
        nb[new_size] = '\0';
        delete[] data_;
        data_ = nb;
        size_ = new_size;
        capacity_ = new_size;
    }
}

void String::push_back(char ch) {
    if (size_ == capacity_) {
        const std::size_t new_cap =
            (capacity_ == 0) ? kDefaultCapacity : capacity_ * 2;
        grow_to(new_cap);  // 分配失败时抛出，*this 保持不变
    }
    data_[size_] = ch;
    ++size_;
    data_[size_] = '\0';
}

// ============================================================================
// 转换为 C 字符串
// ============================================================================

const char* String::c_str() const noexcept {
    static const char kEmpty[] = { '\0' };  // 被移动后对象的兜底空串
    return data_ != nullptr ? data_ : kEmpty;
}

String::operator const char*() const noexcept {
    return c_str();
}

// ============================================================================
// 交换
// ============================================================================

void String::swap(String& other) noexcept {
    char* tmp_data = data_;
    data_ = other.data_;
    other.data_ = tmp_data;

    const std::size_t tmp_size = size_;
    size_ = other.size_;
    other.size_ = tmp_size;

    const std::size_t tmp_cap = capacity_;
    capacity_ = other.capacity_;
    other.capacity_ = tmp_cap;
}

// ============================================================================
// 扩容辅助（强异常安全：先分配、再释放）
// ============================================================================

void String::grow_to(std::size_t new_cap) {
    if (new_cap <= capacity_) {
        return;
    }
    char* nb = new char[new_cap + 1];  // 失败时抛出，对象不变
    for (std::size_t i = 0; i < size_; ++i) {
        nb[i] = data_[i];
    }
    nb[size_] = '\0';
    delete[] data_;
    data_ = nb;
    capacity_ = new_cap;
}

// ============================================================================
// 流操作（bonus，选做）
// ============================================================================

std::ostream& operator<<(std::ostream& os, const String& str) {
    // 按 size() 写出内容（可含 '\0'，与 std::string 语义一致）
    os.write(str.c_str(), static_cast<std::streamsize>(str.size_));
    return os;
}

std::istream& operator>>(std::istream& is, String& str) {
    // sentry：跳过前导空白；若流已坏或没有任何可读字符，它会置 failbit
    std::istream::sentry sentry(is);
    if (!sentry) {
        return is;  // str 保持原值不变
    }

    // 先读到临时对象：只有成功读到内容后才提交给 str，
    // 这样“未读到任何字符”时原值不被破坏。
    String tmp;
    char ch = '\0';
    while (is.get(ch)) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            is.putback(ch);  // 空白符不消费，留给下一次提取
            break;
        }
        tmp.push_back(ch);
    }

    if (tmp.size_ == 0) {
        is.setstate(std::ios::failbit);  // 未读到任何字符
        return is;
    }
    str = std::move(tmp);
    return is;
}

// TODO: 在此实现 include/my_string.h 中声明的所有成员函数与运算符。
