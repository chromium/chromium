// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class NudgeTracker;

// An event representing a 'normal mode' GetUpdate request to the server.
class NormalGetUpdatesRequestEvent : public ProtocolEvent {
 public:
  NormalGetUpdatesRequestEvent(base::Time timestamp,
                               const NudgeTracker& nudge_tracker,
                               const sync_pb::ClientToServerMessage& request);
  NormalGetUpdatesRequestEvent(base::Time timestamp,
                               DataTypeSet nudged_types,
                               DataTypeSet notified_types,
                               DataTypeSet refresh_requested_types,
                               bool is_retry,
                               sync_pb::ClientToServerMessage request);

  NormalGetUpdatesRequestEvent(const NormalGetUpdatesRequestEvent&) = delete;
  NormalGetUpdatesRequestEvent& operator=(const NormalGetUpdatesRequestEvent&) =
      delete;

  ~NormalGetUpdatesRequestEvent() override;

  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  base::Value::Dict GetProtoMessage(bool include_specifics) const override;

  const base::Time timestamp_;

  const DataTypeSet nudged_types_;
  const DataTypeSet notified_types_;
  const DataTypeSet refresh_requested_types_;
  const bool is_retry_;

  const sync_pb::ClientToServerMessage request_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_
