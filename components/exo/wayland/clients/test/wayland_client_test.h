// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENT_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_
#define COMPONENT_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_

#include <memory>

#include "components/exo/wayland/clients/test/wayland_client_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {

class WaylandClientTest : public testing::Test {
 public:
  WaylandClientTest();
  ~WaylandClientTest() override;

  static void SetUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

 protected:
  // Overridden from testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  WaylandClientTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(WaylandClientTest);
};

}  // namespace exo

#endif  // COMPONENT_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_
