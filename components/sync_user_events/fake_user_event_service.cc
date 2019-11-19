// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/fake_user_event_service.h"

#include "components/sync/model/fake_model_type_change_processor.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

FakeUserEventService::FakeUserEventService()
    : fake_bridge_(std::make_unique<FakeModelTypeChangeProcessor>()) {}

FakeUserEventService::~FakeUserEventService() {}

void FakeUserEventService::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  DCHECK(specifics);
  RecordUserEvent(*specifics);
}

void FakeUserEventService::RecordUserEvent(
    const UserEventSpecifics& specifics) {
  recorded_user_events_.push_back(specifics);
}

ModelTypeSyncBridge* FakeUserEventService::GetSyncBridge() {
  return &fake_bridge_;
}

const std::vector<UserEventSpecifics>&
FakeUserEventService::GetRecordedUserEvents() const {
  return recorded_user_events_;
}

}  // namespace syncer
