// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/test_client.h"

namespace exo::wayland::test {

TestClient::TestClient() {
  DETACH_FROM_THREAD(thread_checker_);
}

TestClient::~TestClient() = default;

}  // namespace exo::wayland::test
