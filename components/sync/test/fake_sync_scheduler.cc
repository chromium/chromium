// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_scheduler.h"

#include <utility>

namespace syncer {

FakeSyncScheduler::FakeSyncScheduler() = default;

FakeSyncScheduler::~FakeSyncScheduler() = default;

void FakeSyncScheduler::Start(Mode mode, base::Time last_poll_time) {}

void FakeSyncScheduler::Stop() {}

void FakeSyncScheduler::ScheduleLocalNudge(DataType type) {}

void FakeSyncScheduler::ScheduleLocalRefreshRequest(DataTypeSet types) {}

void FakeSyncScheduler::ScheduleInvalidationNudge(DataType type) {}

void FakeSyncScheduler::SetHasPendingInvalidations(DataType type,
                                                   bool has_invalidation) {}

void FakeSyncScheduler::ScheduleConfiguration(
    sync_pb::SyncEnums::GetUpdatesOrigin origin,
    DataTypeSet types_to_download,
    base::OnceClosure ready_task) {
  std::move(ready_task).Run();
}

void FakeSyncScheduler::ScheduleInitialSyncNudge(DataType data_type) {}

void FakeSyncScheduler::SetNotificationsEnabled(bool notifications_enabled) {}

void FakeSyncScheduler::OnCredentialsUpdated() {}

void FakeSyncScheduler::OnConnectionStatusChange(
    network::mojom::ConnectionType type) {}

void FakeSyncScheduler::OnThrottled(const base::TimeDelta& throttle_duration) {}

void FakeSyncScheduler::OnTypesThrottled(
    DataTypeSet types,
    const base::TimeDelta& throttle_duration) {}

void FakeSyncScheduler::OnTypesBackedOff(DataTypeSet types) {}

bool FakeSyncScheduler::IsAnyThrottleOrBackoff() {
  return false;
}

void FakeSyncScheduler::OnReceivedPollIntervalUpdate(
    const base::TimeDelta& new_interval) {}

void FakeSyncScheduler::OnReceivedCustomNudgeDelays(
    const std::map<DataType, base::TimeDelta>& nudge_delays) {}

void FakeSyncScheduler::OnSyncProtocolError(const SyncProtocolError& error) {}

void FakeSyncScheduler::OnReceivedGuRetryDelay(const base::TimeDelta& delay) {}

void FakeSyncScheduler::OnReceivedMigrationRequest(DataTypeSet types) {}

void FakeSyncScheduler::OnReceivedQuotaParamsForExtensionTypes(
    std::optional<int> max_tokens,
    std::optional<base::TimeDelta> refill_interval,
    std::optional<base::TimeDelta> depleted_quota_nudge_delay) {}

}  // namespace syncer
