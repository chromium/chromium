// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/big_io_buffer.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"

namespace storage {

BigIOBuffer::BigIOBuffer(mojo_base::BigBuffer buffer)
    : net::IOBuffer(buffer), buffer_(std::move(buffer)) {}

BigIOBuffer::BigIOBuffer(size_t size)
    : BigIOBuffer(mojo_base::BigBuffer(size)) {}

BigIOBuffer::~BigIOBuffer() {
  // Must clear raw data pointer in base class so it doesn't hold a dangling
  // ptr.
  ClearSpan();
}

mojo_base::BigBuffer BigIOBuffer::TakeBuffer() {
  ClearSpan();
  return std::move(buffer_);
}

}  // namespace storage
