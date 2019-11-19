// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/obb_mounter/arc_obb_mounter_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

// Singleton factory for ArcObbMounterBridge.
class ArcObbMounterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcObbMounterBridge,
          ArcObbMounterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcObbMounterBridgeFactory";

  static ArcObbMounterBridgeFactory* GetInstance() {
    return base::Singleton<ArcObbMounterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcObbMounterBridgeFactory>;
  ArcObbMounterBridgeFactory() = default;
  ~ArcObbMounterBridgeFactory() override = default;
};

}  // namespace

// static
ArcObbMounterBridge* ArcObbMounterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcObbMounterBridgeFactory::GetForBrowserContext(context);
}

ArcObbMounterBridge::ArcObbMounterBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->obb_mounter()->SetHost(this);
}

ArcObbMounterBridge::~ArcObbMounterBridge() {
  arc_bridge_service_->obb_mounter()->SetHost(nullptr);
}

void ArcObbMounterBridge::MountObb(const std::string& obb_file,
                                   const std::string& target_path,
                                   int32_t owner_gid,
                                   MountObbCallback callback) {
  chromeos::DBusThreadManager::Get()->GetArcObbMounterClient()->MountObb(
      obb_file, target_path, owner_gid, std::move(callback));
}

void ArcObbMounterBridge::UnmountObb(const std::string& target_path,
                                     UnmountObbCallback callback) {
  chromeos::DBusThreadManager::Get()->GetArcObbMounterClient()->UnmountObb(
      target_path, std::move(callback));
}

}  // namespace arc
