// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/proxy_tabs_data_type_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/type_entities_count.h"

namespace sync_sessions {

ProxyTabsDataTypeController::ProxyTabsDataTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    const base::RepeatingCallback<void(State)>& state_changed_cb)
    : DataTypeController(syncer::PROXY_TABS),
      helper_(syncer::PROXY_TABS, sync_service, pref_service),
      state_changed_cb_(state_changed_cb) {}

ProxyTabsDataTypeController::~ProxyTabsDataTypeController() = default;

syncer::DataTypeController::PreconditionState
ProxyTabsDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return helper_.GetPreconditionState();
}

void ProxyTabsDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(configure_context.sync_mode, syncer::SyncMode::kFull);
  state_ = MODEL_LOADED;
  state_changed_cb_.Run(state_);
  model_load_callback.Run(type(), syncer::SyncError());
}

std::unique_ptr<syncer::DataTypeActivationResponse>
ProxyTabsDataTypeController::Connect() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(MODEL_LOADED, state_);
  state_ = RUNNING;
  state_changed_cb_.Run(state_);

  // Set |skip_engine_connection| to true to indicate that, actually, this sync
  // datatype doesn't require communicating to the sync server to upload or
  // download changes.
  auto activation_response =
      std::make_unique<syncer::DataTypeActivationResponse>();
  activation_response->skip_engine_connection = true;
  return activation_response;
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
  std::move(callback).Run(type(), base::Value::List());
}

void ProxyTabsDataTypeController::GetTypeEntitiesCount(
    base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback) const {
  std::move(callback).Run(syncer::TypeEntitiesCount(type()));
}

void ProxyTabsDataTypeController::RecordMemoryUsageAndCountsHistograms() {}

}  // namespace sync_sessions
