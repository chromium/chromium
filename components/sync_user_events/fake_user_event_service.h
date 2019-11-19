// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/sync/model/fake_model_type_sync_bridge.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"

namespace syncer {

// This implementation is intended to be used in unit tests, with public
// accessors that allow reading all data to verify expectations.
class FakeUserEventService : public UserEventService {
 public:
  FakeUserEventService();
  ~FakeUserEventService() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  // TODO(crbug.com/895340): This is hard to mock, replace it (in the base
  // class) by GetControllerDelegate(), then we can get rid of |fake_bridge_|.
  // Maybe we can also expose a raw pointer to be consumed by
  // ForwardingModelTypeControllerDelegate and not care about WeakPtrs anymore
  // (but we need a nice solution for SyncClient).
  ModelTypeSyncBridge* GetSyncBridge() override;

  const std::vector<sync_pb::UserEventSpecifics>& GetRecordedUserEvents() const;

 private:
  std::vector<sync_pb::UserEventSpecifics> recorded_user_events_;
  FakeModelTypeSyncBridge fake_bridge_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserEventService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
