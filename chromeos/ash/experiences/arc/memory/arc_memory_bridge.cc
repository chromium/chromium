// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/memory/arc_memory_bridge.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/mojom/memory.mojom-forward.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/connection_observer.h"
#include "content/public/browser/browser_context.h"

namespace arc {

// Singleton factory for ArcMemoryBridge.
class ArcMemoryBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMemoryBridge,
          ArcMemoryBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMemoryBridgeFactory";

  static ArcMemoryBridgeFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<ArcMemoryBridgeFactory>;
  ArcMemoryBridgeFactory() = default;
  ~ArcMemoryBridgeFactory() override = default;
};

// static
ArcMemoryBridgeFactory* ArcMemoryBridgeFactory::GetInstance() {
  return base::Singleton<ArcMemoryBridgeFactory>::get();
}

// static
ArcMemoryBridge* ArcMemoryBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMemoryBridgeFactory::GetForBrowserContext(context);
}

// static
ArcMemoryBridge* ArcMemoryBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMemoryBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcMemoryBridge::ArcMemoryBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {}

ArcMemoryBridge::~ArcMemoryBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ArcMemoryBridge::DropCaches(DropCachesCallback callback) {
  auto* memory_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->memory(), DropCaches);
  if (!memory_instance) {
    std::move(callback).Run(/*result=*/false);
    return;
  }
  memory_instance->DropCaches(std::move(callback));
}

void ArcMemoryBridge::Reclaim(mojom::ReclaimRequestPtr request,
                              ReclaimCallback callback) {
  auto* const memory_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->memory(), Reclaim);
  if (!memory_instance) {
    std::move(callback).Run(mojom::ReclaimResult::New(0, 0));
    return;
  }
  memory_instance->Reclaim(std::move(request), std::move(callback));
}

// static
void ArcMemoryBridge::EnsureFactoryBuilt() {
  ArcMemoryBridgeFactory::GetInstance();
}

}  // namespace arc
