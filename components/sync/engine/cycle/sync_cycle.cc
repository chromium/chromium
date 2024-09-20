// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/sync_cycle.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/logging.h"
#include "base/observer_list.h"
#include "components/sync/engine/update_handler.h"

namespace syncer {

SyncCycle::SyncCycle(SyncCycleContext* context, Delegate* delegate)
    : context_(context), delegate_(delegate) {
  status_controller_ = std::make_unique<StatusController>();
}

SyncCycle::~SyncCycle() = default;

SyncCycleSnapshot SyncCycle::TakeSnapshot() const {
  return TakeSnapshotWithOrigin(sync_pb::SyncEnums::UNKNOWN_ORIGIN);
}

SyncCycleSnapshot SyncCycle::TakeSnapshotWithOrigin(
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin) const {
  ProgressMarkerMap download_progress_markers;
  for (DataType type : DataTypeSet::All()) {
    const UpdateHandler* update_handler =
        context_->data_type_registry()->GetUpdateHandler(type);
    if (update_handler == nullptr) {
      continue;
    }
    sync_pb::DataTypeProgressMarker progress_marker =
        update_handler->GetDownloadProgress();
    download_progress_markers[type] = progress_marker.SerializeAsString();
  }

  SyncCycleSnapshot snapshot(
      context_->birthday(), context_->bag_of_chips(),
      status_controller_->model_neutral_state(), download_progress_markers,
      delegate_->IsAnyThrottleOrBackoff(),
      status_controller_->num_server_conflicts(),
      context_->notifications_enabled(), status_controller_->sync_start_time(),
      status_controller_->poll_finish_time(), get_updates_origin,
      context_->poll_interval(),
      context_->data_type_registry()->HasUnsyncedItems());

  return snapshot;
}

void SyncCycle::SendSyncCycleEndEventNotification(
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin) {
  SyncCycleEvent event(SyncCycleEvent::SYNC_CYCLE_ENDED);
  event.snapshot = TakeSnapshotWithOrigin(get_updates_origin);

  DVLOG(1) << "Sending cycle end event with snapshot: "
           << event.snapshot.ToString();
  for (SyncEngineEventListener& observer : *context_->listeners()) {
    observer.OnSyncCycleEvent(event);
  }
}

void SyncCycle::SendEventNotification(SyncCycleEvent::EventCause cause) {
  SyncCycleEvent event(cause);
  event.snapshot = TakeSnapshot();

  DVLOG(1) << "Sending event with snapshot: " << event.snapshot.ToString();
  for (SyncEngineEventListener& observer : *context_->listeners()) {
    observer.OnSyncCycleEvent(event);
  }
}

void SyncCycle::SendProtocolEvent(const ProtocolEvent& event) {
  for (SyncEngineEventListener& observer : *context_->listeners()) {
    observer.OnProtocolEvent(event);
  }
}

}  // namespace syncer
