// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/get_updates_delegate.h"

#include "components/sync/engine/events/configure_get_updates_request_event.h"
#include "components/sync/engine/events/normal_get_updates_request_event.h"
#include "components/sync/engine/events/poll_get_updates_request_event.h"
#include "components/sync/engine/get_updates_processor.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/get_updates_caller_info.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

NormalGetUpdatesDelegate::NormalGetUpdatesDelegate(
    const NudgeTracker& nudge_tracker)
    : nudge_tracker_(nudge_tracker) {}

NormalGetUpdatesDelegate::~NormalGetUpdatesDelegate() = default;

// This function assumes the progress markers have already been populated.
void NormalGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  // Set the origin.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::GU_TRIGGER);
  get_updates->set_is_retry(nudge_tracker_->IsRetryRequired());

  // Special case: A GU performed for no other reason than retry will have its
  // origin set to RETRY.
  if (nudge_tracker_->GetOrigin() == sync_pb::SyncEnums::RETRY) {
    get_updates->set_get_updates_origin(sync_pb::SyncEnums::RETRY);
  }

  // Fill in the notification hints.
  for (int i = 0; i < get_updates->from_progress_marker_size(); ++i) {
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->mutable_from_progress_marker(i);
    DataType type =
        GetDataTypeFromSpecificsFieldNumber(progress_marker->data_type_id());

    DCHECK(!nudge_tracker_->IsTypeBlocked(type))
        << "Throttled types should have been removed from the request_types.";

    nudge_tracker_->FillProtoMessage(
        type, progress_marker->mutable_get_update_triggers());
  }
}

std::unique_ptr<ProtocolEvent> NormalGetUpdatesDelegate::GetNetworkRequestEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerMessage& request) const {
  return std::unique_ptr<ProtocolEvent>(
      new NormalGetUpdatesRequestEvent(timestamp, *nudge_tracker_, request));
}

bool NormalGetUpdatesDelegate::IsNotificationInfoRequired() const {
  return true;
}

ConfigureGetUpdatesDelegate::ConfigureGetUpdatesDelegate(
    sync_pb::SyncEnums::GetUpdatesOrigin origin)
    : origin_(origin) {}

ConfigureGetUpdatesDelegate::~ConfigureGetUpdatesDelegate() = default;

void ConfigureGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  get_updates->set_get_updates_origin(origin_);
}

std::unique_ptr<ProtocolEvent>
ConfigureGetUpdatesDelegate::GetNetworkRequestEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerMessage& request) const {
  return std::make_unique<ConfigureGetUpdatesRequestEvent>(timestamp, origin_,
                                                           request);
}

bool ConfigureGetUpdatesDelegate::IsNotificationInfoRequired() const {
  return false;
}

PollGetUpdatesDelegate::PollGetUpdatesDelegate() = default;

PollGetUpdatesDelegate::~PollGetUpdatesDelegate() = default;

void PollGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::PERIODIC);
}

std::unique_ptr<ProtocolEvent> PollGetUpdatesDelegate::GetNetworkRequestEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerMessage& request) const {
  return std::unique_ptr<ProtocolEvent>(
      new PollGetUpdatesRequestEvent(timestamp, request));
}

bool PollGetUpdatesDelegate::IsNotificationInfoRequired() const {
  return false;
}

}  // namespace syncer
