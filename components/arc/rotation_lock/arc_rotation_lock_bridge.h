// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ROTATION_LOCK_ARC_ROTATION_LOCK_BRIDGE_H_
#define COMPONENTS_ARC_ROTATION_LOCK_ARC_ROTATION_LOCK_BRIDGE_H_

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/rotation_lock.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side user rotation lock state to the
// container.
class ArcRotationLockBridge
    : public KeyedService,
      public ConnectionObserver<mojom::RotationLockInstance>,
      public ash::TabletModeObserver,
      public ash::ScreenOrientationController::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcRotationLockBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcRotationLockBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);
  ~ArcRotationLockBridge() override;

  // ConnectionObserver<mojom::RotationLockInstance>:
  void OnConnectionReady() override;

  // ash::ScreenOrientationController::Observer:
  void OnUserRotationLockChanged() override;

  // ash::TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

 private:
  void SendRotationLockState();

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcRotationLockBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ROTATION_LOCK_ARC_ROTATION_LOCK_BRIDGE_H_
