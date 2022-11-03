// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_SYNC_SCHEDULER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_SYNC_SCHEDULER_H_

#include "chromeos/ash/services/device_sync/sync_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace device_sync {

// Mock implementation of SyncScheduler.
class MockSyncScheduler : public SyncScheduler {
 public:
  MockSyncScheduler();

  MockSyncScheduler(const MockSyncScheduler&) = delete;
  MockSyncScheduler& operator=(const MockSyncScheduler&) = delete;

  ~MockSyncScheduler() override;

  // SyncScheduler:
  MOCK_METHOD2(Start,
               void(const base::TimeDelta& elapsed_time_since_last_sync,
                    Strategy strategy));
  MOCK_METHOD0(ForceSync, void(void));
  MOCK_CONST_METHOD0(GetTimeToNextSync, base::TimeDelta(void));
  MOCK_CONST_METHOD0(GetStrategy, Strategy(void));
  MOCK_CONST_METHOD0(GetSyncState, SyncState(void));
  MOCK_METHOD1(OnSyncCompleted, void(bool success));
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_SYNC_SCHEDULER_H_
