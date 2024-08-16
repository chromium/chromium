// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_

#include <memory>
#include <vector>

#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/sync_user_events/user_event_service.h"

namespace syncer {

// This implementation is intended to be used in unit tests, with public
// accessors that allow reading all data to verify expectations.
class FakeUserEventService : public UserEventService {
 public:
  FakeUserEventService();

  FakeUserEventService(const FakeUserEventService&) = delete;
  FakeUserEventService& operator=(const FakeUserEventService&) = delete;

  ~FakeUserEventService() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  const std::vector<sync_pb::UserEventSpecifics>& GetRecordedUserEvents() const;

 private:
  std::vector<sync_pb::UserEventSpecifics> recorded_user_events_;
  FakeDataTypeControllerDelegate fake_controller_delegate_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_FAKE_USER_EVENT_SERVICE_H_
