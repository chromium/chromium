// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/fake_user_event_service.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

FakeUserEventService::FakeUserEventService()
    : fake_controller_delegate_(syncer::USER_EVENTS) {}

FakeUserEventService::~FakeUserEventService() = default;

void FakeUserEventService::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  DCHECK(specifics);
  RecordUserEvent(*specifics);
}

void FakeUserEventService::RecordUserEvent(
    const UserEventSpecifics& specifics) {
  recorded_user_events_.push_back(specifics);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeUserEventService::GetControllerDelegate() {
  return fake_controller_delegate_.GetWeakPtr();
}

const std::vector<UserEventSpecifics>&
FakeUserEventService::GetRecordedUserEvents() const {
  return recorded_user_events_;
}

}  // namespace syncer
