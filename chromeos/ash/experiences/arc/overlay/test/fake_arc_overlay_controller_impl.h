// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_

#include "chromeos/ash/experiences/arc/arc_export.h"
#include "chromeos/ash/experiences/arc/overlay/arc_overlay_controller.h"

namespace ash {

class ARC_EXPORT FakeArcOverlayControllerImpl : public ArcOverlayController {
 public:
  explicit FakeArcOverlayControllerImpl(aura::Window* host_window);
  ~FakeArcOverlayControllerImpl() override;

  // ArcOverlayController:
  void AttachOverlay(aura::Window* overlay_window) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_
