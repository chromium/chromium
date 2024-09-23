// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace sync_pb {
enum SyncEnums_GetUpdatesOrigin : int;
}  // namespace sync_pb

namespace syncer {

// An event representing a configure GetUpdates request to the server.
class ConfigureGetUpdatesRequestEvent : public ProtocolEvent {
 public:
  ConfigureGetUpdatesRequestEvent(
      base::Time timestamp,
      sync_pb::SyncEnums_GetUpdatesOrigin origin,
      const sync_pb::ClientToServerMessage& request);

  ConfigureGetUpdatesRequestEvent(const ConfigureGetUpdatesRequestEvent&) =
      delete;
  ConfigureGetUpdatesRequestEvent& operator=(
      const ConfigureGetUpdatesRequestEvent&) = delete;

  ~ConfigureGetUpdatesRequestEvent() override;

  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  base::Value::Dict GetProtoMessage(bool include_specifics) const override;

  const base::Time timestamp_;
  const sync_pb::SyncEnums_GetUpdatesOrigin origin_;
  const sync_pb::ClientToServerMessage request_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_
