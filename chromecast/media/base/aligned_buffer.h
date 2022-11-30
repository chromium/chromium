// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_ALIGNED_BUFFER_H_
#define CHROMECAST_MEDIA_BASE_ALIGNED_BUFFER_H_

#include <algorithm>
#include <memory>

#include "base/memory/aligned_memory.h"

namespace {
const int kAlignment = 16;
}  // namespace

namespace chromecast {
namespace media {

// Convenient class for 16-byte aligned buffers.
template <typename T>
class AlignedBuffer {
 public:
  AlignedBuffer() : size_(0), data_(nullptr) {}

  explicit AlignedBuffer(size_t size)
      : size_(size),
        data_(static_cast<T*>(
            base::AlignedAlloc(size_ * sizeof(T), kAlignment))) {}

  AlignedBuffer(int size, const T& val) : AlignedBuffer(size) {
    std::fill_n(data_.get(), size_, val);
  }

  // Copy constructor
  AlignedBuffer(const AlignedBuffer& other)
      : size_(other.size()),
        data_(static_cast<T*>(
            base::AlignedAlloc(size_ * sizeof(T), kAlignment))) {
    std::memcpy(data_.get(), other.data(), size_ * sizeof(T));
  }

  ~AlignedBuffer() = default;

  // Assignment operator
  void operator=(const AlignedBuffer& other) {
    size_ = other.size();
    data_.reset(
        static_cast<T*>(base::AlignedAlloc(size_ * sizeof(T), kAlignment)));
    std::memcpy(data_.get(), other.data(), size_ * sizeof(T));
  }

  // Move operator
  AlignedBuffer& operator=(AlignedBuffer&& other) {
    size_ = other.size();
    data_ = std::move(other.data_);
    return *this;
  }

  void resize(size_t new_size) {
    std::unique_ptr<T, base::AlignedFreeDeleter> new_data(
        static_cast<T*>(base::AlignedAlloc(new_size * sizeof(T), kAlignment)));
    size_t size_to_copy = std::min(new_size, size_);
    std::memcpy(new_data.get(), data_.get(), size_to_copy * sizeof(T));
    size_ = new_size;
    data_ = std::move(new_data);
  }

  void assign(size_t n, const T& val) {
    size_ = n;
    data_.reset(
        static_cast<T*>(base::AlignedAlloc(size_ * sizeof(T), kAlignment)));
    std::fill_n(data_.get(), size_, val);
  }

  // Returns a pointer to the underlying data.
  T* data() { return data_.get(); }
  const T* data() const { return data_.get(); }

  T& operator[](size_t i) { return data()[i]; }

  const T& operator[](size_t i) const { return data()[i]; }

  // Returns number of elements.
  size_t size() const { return size_; }

  bool empty() const { return size_ == 0; }

 private:
  size_t size_;
  std::unique_ptr<T, base::AlignedFreeDeleter> data_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_ALIGNED_BUFFER_H_
