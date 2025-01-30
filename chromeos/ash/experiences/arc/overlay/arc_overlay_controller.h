// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_

#include "chromeos/ash/experiences/arc/arc_export.h"

namespace aura {
class Window;
}

namespace ash {

class ARC_EXPORT ArcOverlayController {
 public:
  ArcOverlayController();
  virtual ~ArcOverlayController();

  // Attaches the window that is intended to be used as the overlay.
  // This is expected to be a toplevel window, and it will be reparented.
  virtual void AttachOverlay(aura::Window* overlay_window) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
