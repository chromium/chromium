// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WLCS_POINTER_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WLCS_POINTER_H_

#include "base/memory/raw_ptr.h"
#include "third_party/wlcs/src/include/wlcs/pointer.h"

namespace exo::wlcs {

class DisplayServer;

// Allows WLCS to drive touch events during test execution.
class Pointer : public WlcsPointer {
 public:
  explicit Pointer(DisplayServer* server);

  // This object is not movable or copyable.
  Pointer(const Pointer&) = delete;
  Pointer& operator=(const Pointer&) = delete;

  ~Pointer();

  void MoveAbsolute(wl_fixed_t x, wl_fixed_t y);
  void MoveRelative(wl_fixed_t x, wl_fixed_t y);
  void ButtonUp(int button);
  void ButtonDown(int button);

 private:
  const raw_ptr<DisplayServer> server_;
};

}  // namespace exo::wlcs

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WLCS_POINTER_H_
