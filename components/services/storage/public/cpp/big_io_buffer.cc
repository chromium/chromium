// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/big_io_buffer.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"

namespace storage {

BigIOBuffer::BigIOBuffer(mojo_base::BigBuffer buffer)
    : net::IOBufferWithSize(nullptr, buffer.size()),
      buffer_(std::move(buffer)) {
  data_ = reinterpret_cast<char*>(buffer_.data());
}

BigIOBuffer::BigIOBuffer(size_t size)
    : net::IOBufferWithSize(nullptr, size),
      buffer_(mojo_base::BigBuffer(size)) {
  data_ = reinterpret_cast<char*>(buffer_.data());
  DCHECK(data_);
}

BigIOBuffer::~BigIOBuffer() {
  // Must clear `data_` so base class doesn't attempt to delete[] a pointer
  // it doesn't own.
  data_ = nullptr;
  size_ = 0UL;
}

mojo_base::BigBuffer BigIOBuffer::TakeBuffer() {
  data_ = nullptr;
  size_ = 0UL;
  return std::move(buffer_);
}

}  // namespace storage
