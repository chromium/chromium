// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/test_client.h"

#include "base/logging.h"

namespace exo::wayland::test {

TestClient::TestClient() {
  DETACH_FROM_THREAD(thread_checker_);
}

TestClient::~TestClient() = default;

bool TestClient::Init(const std::string& wayland_socket,
                      base::flat_map<std::string, uint32_t> global_versions) {
  display_.reset(wl_display_connect(wayland_socket.c_str()));
  if (!display_) {
    LOG(ERROR) << "wl_display_connect() failed.";
    return false;
  }

  globals_.Init(display_.get(), std::move(global_versions));
  return true;
}

bool TestClient::InitShmBufferFactory(int32_t pool_size) {
  if (shm_buffer_factory_)
    return false;

  shm_buffer_factory_ = std::make_unique<ShmBufferFactory>();
  if (!shm_buffer_factory_->Init(shm(), pool_size)) {
    shm_buffer_factory_.reset();
    return false;
  }

  return true;
}

}  // namespace exo::wayland::test
