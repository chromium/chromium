// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_

#include "base/task/single_thread_task_runner.h"
#include "components/exo/wayland/clients/test/wayland_client_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {

class WaylandClientTest : public testing::Test {
 public:
  WaylandClientTest();

  WaylandClientTest(const WaylandClientTest&) = delete;
  WaylandClientTest& operator=(const WaylandClientTest&) = delete;

  ~WaylandClientTest() override;

  static void SetUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

 protected:
  // Overridden from testing::Test:
  void SetUp() override;
  void TearDown() override;

  wayland::Server* GetServer();

 private:
  WaylandClientTestHelper test_helper_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_H_
