// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/safety/arc_safety_bridge.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

// Singleton factory for ArcSafetyBridge
class ArcSafetyBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSafetyBridge,
          ArcSafetyBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSafetyBridgeFactory";

  static ArcSafetyBridgeFactory* GetInstance() {
    return base::Singleton<ArcSafetyBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSafetyBridgeFactory>;
  ArcSafetyBridgeFactory() = default;
  ~ArcSafetyBridgeFactory() override = default;
};

}  // namespace

// static
ArcSafetyBridge* ArcSafetyBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSafetyBridgeFactory::GetForBrowserContext(context);
}

// static
ArcSafetyBridge* ArcSafetyBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSafetyBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcSafetyBridge::ArcSafetyBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->on_device_safety()->SetHost(this);
}

ArcSafetyBridge::~ArcSafetyBridge() {
  arc_bridge_service_->on_device_safety()->SetHost(nullptr);
}

// static
void ArcSafetyBridge::EnsureFactoryBuilt() {
  ArcSafetyBridgeFactory::GetInstance();
}

void ArcSafetyBridge::IsCrosSafetyServiceEnabled(
    IsCrosSafetyServiceEnabledCallback callback) {
  std::move(callback).Run(ash::features::IsCrosSafetyServiceEnabled());
}

}  // namespace arc
