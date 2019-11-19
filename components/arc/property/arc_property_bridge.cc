// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/property/arc_property_bridge.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {
namespace {

// Singleton factory for ArcPropertyBridge.
class ArcPropertyBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPropertyBridge,
          ArcPropertyBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPropertyBridgeFactory";

  static ArcPropertyBridgeFactory* GetInstance() {
    return base::Singleton<ArcPropertyBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPropertyBridgeFactory>;
  ArcPropertyBridgeFactory() = default;
  ~ArcPropertyBridgeFactory() override = default;
};

}  // namespace

// static
ArcPropertyBridge* ArcPropertyBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPropertyBridgeFactory::GetForBrowserContext(context);
}

ArcPropertyBridge::ArcPropertyBridge(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->property()->AddObserver(this);
}

ArcPropertyBridge::~ArcPropertyBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->property()->RemoveObserver(this);
}

void ArcPropertyBridge::OnConnectionReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojom::PropertyInstance* property_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->property(), GetGcaMigrationProperty);
  DCHECK(property_instance);

  for (auto& pending_request : pending_requests_) {
    property_instance->GetGcaMigrationProperty(std::move(pending_request));
  }
  pending_requests_.clear();
}

void ArcPropertyBridge::GetGcaMigrationProperty(
    mojom::PropertyInstance::GetGcaMigrationPropertyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojom::PropertyInstance* property_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->property(), GetGcaMigrationProperty);
  if (!property_instance) {
    pending_requests_.emplace_back(std::move(callback));
    return;
  }

  property_instance->GetGcaMigrationProperty(std::move(callback));
}

}  // namespace arc
