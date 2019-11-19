// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/lock_screen/arc_lock_screen_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/session_manager/core/session_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcLockScreenBridge.
class ArcLockScreenBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcLockScreenBridge,
          ArcLockScreenBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcLockScreenBridgeFactory";

  static ArcLockScreenBridgeFactory* GetInstance() {
    return base::Singleton<ArcLockScreenBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcLockScreenBridgeFactory>;
  ArcLockScreenBridgeFactory() = default;
  ~ArcLockScreenBridgeFactory() override = default;
};

}  // namespace

// static
ArcLockScreenBridge* ArcLockScreenBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcLockScreenBridgeFactory::GetForBrowserContext(context);
}

ArcLockScreenBridge::ArcLockScreenBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->lock_screen()->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
}

ArcLockScreenBridge::~ArcLockScreenBridge() {
  arc_bridge_service_->lock_screen()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void ArcLockScreenBridge::OnConnectionReady() {
  SendDeviceLockedState();
}

void ArcLockScreenBridge::OnSessionStateChanged() {
  SendDeviceLockedState();
}

void ArcLockScreenBridge::SendDeviceLockedState() {
  mojom::LockScreenInstance* lock_screen_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->lock_screen(), SetDeviceLocked);
  if (!lock_screen_instance)
    return;
  lock_screen_instance->SetDeviceLocked(
      session_manager::SessionManager::Get()->IsUserSessionBlocked());
}

}  // namespace arc
