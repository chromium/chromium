// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WLCS_TOUCH_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WLCS_TOUCH_H_

#include "base/memory/raw_ptr.h"
#include "third_party/wlcs/src/include/wlcs/touch.h"

namespace exo::wlcs {

class DisplayServer;

// Allows WLCS to drive touch events during test execution.
class Touch : public WlcsTouch {
 public:
  explicit Touch(DisplayServer* server);

  // This object is not movable or copyable.
  Touch(const Touch&) = delete;
  Touch& operator=(const Touch&) = delete;

  ~Touch();

  void TouchDown(wl_fixed_t x, wl_fixed_t y);
  void TouchMove(wl_fixed_t x, wl_fixed_t y);
  void TouchUp();

 private:
  const raw_ptr<DisplayServer> server_;
};

}  // namespace exo::wlcs

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WLCS_TOUCH_H_
