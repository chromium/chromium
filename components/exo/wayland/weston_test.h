// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_
#define COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_

#include <memory>

#include "base/component_export.h"

struct wl_display;
namespace exo {
namespace wayland {

class COMPONENT_EXPORT(WESTON_TEST) WestonTest {
 public:
  explicit WestonTest(wl_display* display);
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
