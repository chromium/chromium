// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_OBSERVER_H_
#define COMPONENTS_EXO_SHELL_SURFACE_OBSERVER_H_

#include <stdint.h>

#include "base/observer_list_types.h"

namespace exo {

class ShellSurfaceObserver : public base::CheckedObserver {
 public:
  // Invoked to notify the configure event with `serial`.
  virtual void OnConfigure(uint32_t serial) {}

  // Invoked to notify an acknowledgement of the configure with `serial`.
  virtual void OnAcknowledgeConfigure(uint32_t serial) {}

  // Invoked when underlying ShellSurface goes away.
  virtual void OnShellSurfaceDestroyed() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_OBSERVER_H_
