// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/app_permissions/arc_app_permissions_bridge.h"

#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

class ArcAppPermissionsBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAppPermissionsBridge,
          ArcAppPermissionsBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAppPermissionsBridgeFactory";

  static ArcAppPermissionsBridgeFactory* GetInstance() {
    return base::Singleton<ArcAppPermissionsBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAppPermissionsBridgeFactory>;
  ArcAppPermissionsBridgeFactory() = default;
  ~ArcAppPermissionsBridgeFactory() override = default;
};

}  // namespace

// static
ArcAppPermissionsBridge* ArcAppPermissionsBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAppPermissionsBridgeFactory::GetForBrowserContext(context);
}

ArcAppPermissionsBridge::ArcAppPermissionsBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service) {}

ArcAppPermissionsBridge::~ArcAppPermissionsBridge() = default;

}  // namespace arc
