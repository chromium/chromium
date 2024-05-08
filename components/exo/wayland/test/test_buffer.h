// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_TEST_BUFFER_H_
#define COMPONENTS_EXO_WAYLAND_TEST_TEST_BUFFER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo::wayland::test {

class BufferListener {
 public:
  virtual ~BufferListener() = default;
  virtual void OnRelease(wl_buffer* buffer) = 0;
};

class TestBuffer {
 public:
  explicit TestBuffer(
      std::unique_ptr<wl_buffer, decltype(&wl_buffer_destroy)> resource);

  TestBuffer(const TestBuffer&) = delete;
  TestBuffer& operator=(const TestBuffer&) = delete;

  ~TestBuffer();

  // `listener` must outlive this object.
  void SetListener(BufferListener* listener);

  wl_buffer* resource() { return resource_.get(); }

  wl_buffer* GetResourceAndRelease() { return resource_.release(); }

  static void OnRelease(void* data, wl_buffer* resource);

 private:
  std::unique_ptr<wl_buffer, decltype(&wl_buffer_destroy)> resource_;
  raw_ptr<BufferListener> listener_ = nullptr;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_TEST_BUFFER_H_
