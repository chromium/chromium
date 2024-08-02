// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/no_op_user_event_service.h"

#include "base/memory/weak_ptr.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

NoOpUserEventService::NoOpUserEventService() = default;

NoOpUserEventService::~NoOpUserEventService() = default;

void NoOpUserEventService::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {}

void NoOpUserEventService::RecordUserEvent(
    const UserEventSpecifics& specifics) {}

base::WeakPtr<syncer::DataTypeControllerDelegate>
NoOpUserEventService::GetControllerDelegate() {
  return nullptr;
}

}  // namespace syncer
