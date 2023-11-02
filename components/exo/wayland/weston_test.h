// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_
#define COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_

#include <memory>

#include "base/component_export.h"

namespace exo {
namespace wayland {
class Server;

class COMPONENT_EXPORT(WESTON_TEST) WestonTest {
 public:
  explicit WestonTest(Server* server);
  WestonTest(const WestonTest&) = delete;
  WestonTest& operator=(const WestonTest&) = delete;
  ~WestonTest();

  struct WestonTestState;

 private:
  std::unique_ptr<WestonTestState> data_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_
