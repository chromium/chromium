// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/test_buffer.h"

#include "base/check_op.h"

namespace exo::wayland::test {
namespace {

wl_buffer_listener buffer_listener = {TestBuffer::OnRelease};

}  // namespace

TestBuffer::TestBuffer(
    std::unique_ptr<wl_buffer, decltype(&wl_buffer_destroy)> resource)
    : resource_(std::move(resource)) {}

TestBuffer::~TestBuffer() = default;

void TestBuffer::SetListener(BufferListener* listener) {
  DCHECK(!listener_);
  DCHECK(listener);
  listener_ = listener;

  wl_buffer_add_listener(resource_.get(), &buffer_listener, this);
}

// static
void TestBuffer::OnRelease(void* data, wl_buffer* resource) {
  TestBuffer* buffer = static_cast<TestBuffer*>(data);

  DCHECK_EQ(buffer->resource(), resource);
  DCHECK(buffer->listener_);
  buffer->listener_->OnRelease(resource);
}

}  // namespace exo::wayland::test
