// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/test/wayland_client_test.h"

namespace exo {

WaylandClientTest::WaylandClientTest() = default;
WaylandClientTest::~WaylandClientTest() = default;

// Static
void WaylandClientTest::SetUIThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  WaylandClientTestHelper::SetUIThreadTaskRunner(
      std::move(ui_thread_task_runner));
}

void WaylandClientTest::SetUp() {
  test_helper_.SetUp();
}

void WaylandClientTest::TearDown() {
  test_helper_.TearDown();
}

}  // namespace exo
