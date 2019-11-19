// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_
#define COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/lock_screen.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side lock screen state to the container.
class ArcLockScreenBridge
    : public KeyedService,
      public ConnectionObserver<mojom::LockScreenInstance>,
      public session_manager::SessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcLockScreenBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcLockScreenBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcLockScreenBridge() override;

  // ConnectionObserver<mojom::LockScreenInstance> overrides:
  void OnConnectionReady() override;

  // session_manager::SessionManagerObserver overrides.
  void OnSessionStateChanged() override;

 private:
  // Sends the device locked state to container.
  void SendDeviceLockedState();

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcLockScreenBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_
