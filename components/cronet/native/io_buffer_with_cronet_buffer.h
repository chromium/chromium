// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_IO_BUFFER_WITH_CRONET_BUFFER_H_
#define COMPONENTS_CRONET_NATIVE_IO_BUFFER_WITH_CRONET_BUFFER_H_

#include <memory>

#include "components/cronet/native/generated/cronet.idl_c.h"
#include "net/base/io_buffer.h"

namespace cronet {

// net::WrappedIOBuffer subclass for a buffer owned by a Cronet_Buffer.
// Keeps the Cronet_Buffer alive until destroyed or released.
// Uses WrappedIOBuffer because data() is owned by the Cronet_Buffer.
class IOBufferWithCronet_Buffer : public net::WrappedIOBuffer {
 public:
  // Creates a buffer that takes ownership of the Cronet_Buffer.
  explicit IOBufferWithCronet_Buffer(Cronet_BufferPtr cronet_buffer);

  IOBufferWithCronet_Buffer(const IOBufferWithCronet_Buffer&) = delete;
  IOBufferWithCronet_Buffer& operator=(const IOBufferWithCronet_Buffer&) =
      delete;

  // Releases ownership of |cronet_buffer_| and returns it to caller.
  Cronet_BufferPtr Release();

 private:
  ~IOBufferWithCronet_Buffer() override;

  // Cronet buffer owned by |this|.
  std::unique_ptr<Cronet_Buffer> cronet_buffer_;
};

// Represents a Cronet_Buffer backed by a net::IOBuffer. Keeps both the
// net::IOBuffer and the Cronet_Buffer object alive until destroyed.
class Cronet_BufferWithIOBuffer {
 public:
  Cronet_BufferWithIOBuffer(scoped_refptr<net::IOBuffer> io_buffer,
                            size_t io_buffer_len);

  Cronet_BufferWithIOBuffer(const Cronet_BufferWithIOBuffer&) = delete;
  Cronet_BufferWithIOBuffer& operator=(const Cronet_BufferWithIOBuffer&) =
      delete;

  ~Cronet_BufferWithIOBuffer();

  const net::IOBuffer* io_buffer() const { return io_buffer_.get(); }
  size_t io_buffer_len() const { return io_buffer_len_; }

  // Returns pointer to Cronet buffer owned by |this|.
  Cronet_BufferPtr cronet_buffer() {
    CHECK(io_buffer_->HasAtLeastOneRef());
    return cronet_buffer_.get();
  }

 private:
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_len_;

  // Cronet buffer owned by |this|.
  std::unique_ptr<Cronet_Buffer> cronet_buffer_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_IO_BUFFER_WITH_CRONET_BUFFER_H_
