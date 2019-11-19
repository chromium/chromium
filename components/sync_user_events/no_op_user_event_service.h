// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/sync_user_events/user_event_service.h"

namespace syncer {

class ModelTypeSyncBridge;

// This implementation is used when we know event should never be recorded,
// such as in incognito mode.
class NoOpUserEventService : public UserEventService {
 public:
  NoOpUserEventService();
  ~NoOpUserEventService() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  ModelTypeSyncBridge* GetSyncBridge() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpUserEventService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_
