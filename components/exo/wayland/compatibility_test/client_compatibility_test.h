// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_CLIENT_COMPATIBILITY_TEST_H_
#define COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_CLIENT_COMPATIBILITY_TEST_H_

#include "components/exo/wayland/clients/test/wayland_client_test.h"

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {

class ClientCompatibilityTest : public WaylandClientTest {
 public:
  ClientCompatibilityTest();

  ClientCompatibilityTest(const ClientCompatibilityTest&) = delete;
  ClientCompatibilityTest& operator=(const ClientCompatibilityTest&) = delete;

  ~ClientCompatibilityTest() override;
};

}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_CLIENT_COMPATIBILITY_TEST_H_
