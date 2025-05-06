// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_TEST_CONTROLLER_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_TEST_CONTROLLER_H_

#include <memory>

namespace exo::wayland {

class Server;

class TestController {
 public:
  TestController(Server* server);
  TestController(const TestController&) = delete;
  TestController& operator=(const TestController&) = delete;
  ~TestController();
  // State is declared here but not defined because its definition references
  // symbols in test-only dependencies.
  struct State;

 private:
  std::unique_ptr<State> state_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_TEST_CONTROLLER_H_
