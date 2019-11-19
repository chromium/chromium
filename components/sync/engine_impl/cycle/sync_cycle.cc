// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/sync_cycle.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "components/sync/engine_impl/update_handler.h"
#include "components/sync/syncable/directory.h"

namespace syncer {

SyncCycle::SyncCycle(SyncCycleContext* context, Delegate* delegate)
    : context_(context), delegate_(delegate) {
  status_controller_ = std::make_unique<StatusController>();
}

SyncCycle::~SyncCycle() {}

SyncCycleSnapshot SyncCycle::TakeSnapshot() const {
  return TakeSnapshotWithOrigin(sync_pb::SyncEnums::UNKNOWN_ORIGIN);
}

SyncCycleSnapshot SyncCycle::TakeSnapshotWithOrigin(
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin) const {
  ProgressMarkerMap download_progress_markers;
  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
    ModelType type(ModelTypeFromInt(i));
    const UpdateHandler* update_handler =
        context_->model_type_registry()->GetUpdateHandler(type);
    if (update_handler == nullptr) {
      continue;
    }
    sync_pb::DataTypeProgressMarker progress_marker;
    update_handler->GetDownloadProgress(&progress_marker);
    download_progress_markers[type] = progress_marker.SerializeAsString();
  }

  // TODO(mastiz): Remove dependency to directory, since it most likely hides
  // an issue with USS types.
  syncable::Directory* dir = context_->directory();

  std::vector<int> num_entries_by_type(ModelType::NUM_ENTRIES, 0);
  std::vector<int> num_to_delete_entries_by_type(ModelType::NUM_ENTRIES, 0);
  dir->CollectMetaHandleCounts(&num_entries_by_type,
                               &num_to_delete_entries_by_type);

  SyncCycleSnapshot snapshot(
      context_->birthday(), context_->bag_of_chips(),
      status_controller_->model_neutral_state(), download_progress_markers,
      delegate_->IsAnyThrottleOrBackoff(),
      status_controller_->num_encryption_conflicts(),
      status_controller_->num_hierarchy_conflicts(),
      status_controller_->num_server_conflicts(),
      context_->notifications_enabled(), dir->GetEntriesCount(),
      status_controller_->sync_start_time(),
      status_controller_->poll_finish_time(), num_entries_by_type,
      num_to_delete_entries_by_type, get_updates_origin,
      context_->poll_interval(),
      context_->model_type_registry()->HasUnsyncedItems());

  return snapshot;
}

void SyncCycle::SendSyncCycleEndEventNotification(
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin) {
  SyncCycleEvent event(SyncCycleEvent::SYNC_CYCLE_ENDED);
  event.snapshot = TakeSnapshotWithOrigin(get_updates_origin);

  DVLOG(1) << "Sending cycle end event with snapshot: "
           << event.snapshot.ToString();
  for (auto& observer : *context_->listeners())
    observer.OnSyncCycleEvent(event);
}

void SyncCycle::SendEventNotification(SyncCycleEvent::EventCause cause) {
  SyncCycleEvent event(cause);
  event.snapshot = TakeSnapshot();

  DVLOG(1) << "Sending event with snapshot: " << event.snapshot.ToString();
  for (auto& observer : *context_->listeners())
    observer.OnSyncCycleEvent(event);
}

void SyncCycle::SendProtocolEvent(const ProtocolEvent& event) {
  for (auto& observer : *context_->listeners())
    observer.OnProtocolEvent(event);
}

}  // namespace syncer
