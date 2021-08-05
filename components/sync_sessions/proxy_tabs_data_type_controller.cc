// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/proxy_tabs_data_type_controller.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/model/type_entities_count.h"

namespace sync_sessions {

ProxyTabsDataTypeController::ProxyTabsDataTypeController(
    const base::RepeatingCallback<void(State)>& state_changed_cb)
    : DataTypeController(syncer::PROXY_TABS),
      state_changed_cb_(state_changed_cb),
      state_(NOT_RUNNING) {}

ProxyTabsDataTypeController::~ProxyTabsDataTypeController() {}

void ProxyTabsDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(configure_context.sync_mode, syncer::SyncMode::kFull);
  // Bypass connection to the sync engine by reporting that it's already
  // running.
  state_ = RUNNING;
  state_changed_cb_.Run(state_);
  model_load_callback.Run(type(), syncer::SyncError());
}

std::unique_ptr<syncer::DataTypeActivationResponse>
ProxyTabsDataTypeController::Connect() {
  // This controller never enters the MODEL_LOADED state.
  NOTREACHED();
  return nullptr;
}

void ProxyTabsDataTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  state_ = NOT_RUNNING;
  state_changed_cb_.Run(state_);
  std::move(callback).Run();
}

syncer::DataTypeController::State ProxyTabsDataTypeController::state() const {
  return state_;
}

bool ProxyTabsDataTypeController::ShouldRunInTransportOnlyMode() const {
  return false;
}

void ProxyTabsDataTypeController::GetAllNodes(AllNodesCallback callback) {
  std::move(callback).Run(type(), std::make_unique<base::ListValue>());
}

void ProxyTabsDataTypeController::GetTypeEntitiesCount(
    base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback) const {
  std::move(callback).Run(syncer::TypeEntitiesCount(type()));
}

void ProxyTabsDataTypeController::RecordMemoryUsageAndCountsHistograms() {}

}  // namespace sync_sessions
