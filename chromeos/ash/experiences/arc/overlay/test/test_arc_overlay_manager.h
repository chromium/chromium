// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_

#include "chromeos/ash/experiences/arc/arc_export.h"
#include "chromeos/ash/experiences/arc/overlay/arc_overlay_manager.h"

namespace ash {

class ARC_EXPORT TestArcOverlayManager : public ArcOverlayManager {
 public:
  TestArcOverlayManager();
  ~TestArcOverlayManager() override;

  // ArcOverlayManager:
  std::unique_ptr<ArcOverlayController> CreateController(
      aura::Window* host_window) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_
