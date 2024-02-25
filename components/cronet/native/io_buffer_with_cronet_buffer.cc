// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/io_buffer_with_cronet_buffer.h"

#include "base/no_destructor.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

namespace {
// Implementation of Cronet_BufferCallback that doesn't free the data as it
// is not owned by the buffer.
class Cronet_BufferCallbackUnowned : public Cronet_BufferCallback {
 public:
  Cronet_BufferCallbackUnowned() = default;

  Cronet_BufferCallbackUnowned(const Cronet_BufferCallbackUnowned&) = delete;
  Cronet_BufferCallbackUnowned& operator=(const Cronet_BufferCallbackUnowned&) =
      delete;

  ~Cronet_BufferCallbackUnowned() override = default;

  void OnDestroy(Cronet_BufferPtr buffer) override {}
};

}  // namespace

namespace cronet {

IOBufferWithCronet_Buffer::IOBufferWithCronet_Buffer(
    Cronet_BufferPtr cronet_buffer)
    : net::WrappedIOBuffer(
          base::make_span(static_cast<const char*>(cronet_buffer->GetData()),
                          static_cast<size_t>(cronet_buffer->GetSize()))),
      cronet_buffer_(cronet_buffer) {}

IOBufferWithCronet_Buffer::~IOBufferWithCronet_Buffer() {
  if (cronet_buffer_) {
    Cronet_Buffer_Destroy(Release());
  }
}

Cronet_BufferPtr IOBufferWithCronet_Buffer::Release() {
  data_ = nullptr;  // Avoid dangling pointer.
  return cronet_buffer_.release();
}

Cronet_BufferWithIOBuffer::Cronet_BufferWithIOBuffer(
    scoped_refptr<net::IOBuffer> io_buffer,
    size_t io_buffer_len)
    : io_buffer_(std::move(io_buffer)),
      io_buffer_len_(io_buffer_len),
      cronet_buffer_(Cronet_Buffer_Create()) {
  static base::NoDestructor<Cronet_BufferCallbackUnowned> static_callback;
  cronet_buffer_->InitWithDataAndCallback(io_buffer_->data(), io_buffer_len_,
                                          static_callback.get());
}

Cronet_BufferWithIOBuffer::~Cronet_BufferWithIOBuffer() = default;

}  // namespace cronet
