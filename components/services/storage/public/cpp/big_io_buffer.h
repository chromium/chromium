// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BIG_IO_BUFFER_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BIG_IO_BUFFER_H_

#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"

namespace storage {

// A net::IOBufferWithSize backed by a mojo_base::BigBuffer. Avoids having to
// copy an IOBuffer to a BigBuffer to return the results of an IO operation.
class COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC) BigIOBuffer
    : public net::IOBuffer {
 public:
  BigIOBuffer(const BigIOBuffer&) = delete;
  BigIOBuffer& operator=(const BigIOBuffer&) = delete;
  explicit BigIOBuffer(mojo_base::BigBuffer buffer);
  explicit BigIOBuffer(size_t size);
  mojo_base::BigBuffer TakeBuffer();

 protected:
  ~BigIOBuffer() override;

 private:
  mojo_base::BigBuffer buffer_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BIG_IO_BUFFER_H_
