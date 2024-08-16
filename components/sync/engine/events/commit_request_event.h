// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_COMMIT_REQUEST_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_COMMIT_REQUEST_EVENT_H_

#include <cstddef>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a commit request message sent to the server.
class CommitRequestEvent : public ProtocolEvent {
 public:
  CommitRequestEvent(base::Time timestamp,
                     size_t num_items,
                     DataTypeSet contributing_types,
                     const sync_pb::ClientToServerMessage& request);

  CommitRequestEvent(const CommitRequestEvent&) = delete;
  CommitRequestEvent& operator=(const CommitRequestEvent&) = delete;

  ~CommitRequestEvent() override;

  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  base::Value::Dict GetProtoMessage(bool include_specifics) const override;
  const base::Time timestamp_;
  const size_t num_items_;
  const DataTypeSet contributing_types_;
  const sync_pb::ClientToServerMessage request_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_COMMIT_REQUEST_EVENT_H_
