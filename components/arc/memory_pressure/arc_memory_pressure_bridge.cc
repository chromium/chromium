// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/memory_pressure/arc_memory_pressure_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {
namespace {

// Singleton factory for ArcMemoryPressureBridge.
class ArcMemoryPressureBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMemoryPressureBridge,
          ArcMemoryPressureBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMemoryPressureBridgeFactory";

  static ArcMemoryPressureBridgeFactory* GetInstance() {
    return base::Singleton<ArcMemoryPressureBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMemoryPressureBridgeFactory>;

  ArcMemoryPressureBridgeFactory() {
    DependsOn(ArcMetricsService::GetFactory());
  }

  ~ArcMemoryPressureBridgeFactory() override = default;
};

}  // namespace

// static
ArcMemoryPressureBridge* ArcMemoryPressureBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMemoryPressureBridgeFactory::GetForBrowserContext(context);
}

ArcMemoryPressureBridge*
ArcMemoryPressureBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMemoryPressureBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcMemoryPressureBridge::ArcMemoryPressureBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      arc_metrics_service_(ArcMetricsService::GetForBrowserContext(context)) {
  DCHECK(arc_metrics_service_ != nullptr);
  chromeos::ResourcedClient* client = chromeos::ResourcedClient::Get();
  DCHECK(client);
  client->AddArcVmObserver(this);
}

ArcMemoryPressureBridge::~ArcMemoryPressureBridge() {
  chromeos::ResourcedClient* client = chromeos::ResourcedClient::Get();
  if (client)
    client->RemoveArcVmObserver(this);
}

void ArcMemoryPressureBridge::OnMemoryPressure(
    chromeos::ResourcedClient::PressureLevelArcVm level,
    uint64_t reclaim_target_kb) {
  LOG(ERROR) << "ArcMemoryPressureBridge::OnMemoryPressure("
             << static_cast<int>(level) << ", " << reclaim_target_kb << ")";
  if (memory_pressure_in_flight_)
    return;
  memory_pressure_in_flight_ = true;
  mojom::ProcessState arc_level;
  switch (level) {
    case chromeos::ResourcedClient::PressureLevelArcVm::NONE:
      return;

    case chromeos::ResourcedClient::PressureLevelArcVm::CACHED:
      arc_level = mojom::ProcessState::R_CACHED_ACTIVITY_CLIENT;
      break;

    case chromeos::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE:
    case chromeos::ResourcedClient::PressureLevelArcVm::FOREGROUND:
      arc_level = mojom::ProcessState::R_TOP;
      break;
  }
  auto* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), ApplyHostMemoryPressure);
  if (!process_instance) {
    LOG(ERROR) << "ArcMemoryPressureBridge::OnMemoryPressure event, but no "
                  "process_instance";
    return;
  }
  process_instance->ApplyHostMemoryPressure(
      arc_level, reclaim_target_kb * 1024,
      base::BindOnce(&ArcMemoryPressureBridge::OnHostMemoryPressureComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcMemoryPressureBridge::OnHostMemoryPressureComplete(uint32_t killed,
                                                           uint64_t reclaimed) {
  DCHECK(memory_pressure_in_flight_);
  memory_pressure_in_flight_ = false;
  arc_metrics_service_->ReportMemoryPressureArcVmKills(killed,
                                                       reclaimed / 1024);
  LOG(ERROR) << "ARC Killed " << killed << " processes to reclaim " << reclaimed
             << " bytes";
}

}  // namespace arc
