// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/syncer_error.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a GetUpdates response event from the server.
//
// Unlike the events for the request message, the response events are generic
// and do not vary for each type of GetUpdate cycle.
class GetUpdatesResponseEvent : public ProtocolEvent {
 public:
  GetUpdatesResponseEvent(base::Time timestamp,
                          const sync_pb::ClientToServerResponse& response,
                          SyncerError error);

  GetUpdatesResponseEvent(const GetUpdatesResponseEvent&) = delete;
  GetUpdatesResponseEvent& operator=(const GetUpdatesResponseEvent&) = delete;

  ~GetUpdatesResponseEvent() override;

  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  base::Value::Dict GetProtoMessage(bool include_specifics) const override;

  const base::Time timestamp_;
  const sync_pb::ClientToServerResponse response_;
  const SyncerError error_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_
