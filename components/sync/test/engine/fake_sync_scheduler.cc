// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/fake_sync_scheduler.h"

namespace syncer {

FakeSyncScheduler::FakeSyncScheduler() {}

FakeSyncScheduler::~FakeSyncScheduler() {}

void FakeSyncScheduler::Start(Mode mode, base::Time last_poll_time) {}

void FakeSyncScheduler::Stop() {}

void FakeSyncScheduler::ScheduleLocalNudge(
    ModelTypeSet types,
    const base::Location& nudge_location) {}

void FakeSyncScheduler::ScheduleLocalRefreshRequest(
    ModelTypeSet types,
    const base::Location& nudge_location) {}

void FakeSyncScheduler::ScheduleInvalidationNudge(
    ModelType type,
    std::unique_ptr<InvalidationInterface> interface,
    const base::Location& nudge_location) {}

void FakeSyncScheduler::ScheduleConfiguration(
    const ConfigurationParams& params) {
  params.ready_task.Run();
}

void FakeSyncScheduler::ScheduleInitialSyncNudge(ModelType model_type) {}

void FakeSyncScheduler::SetNotificationsEnabled(bool notifications_enabled) {}

void FakeSyncScheduler::OnCredentialsUpdated() {}

void FakeSyncScheduler::OnConnectionStatusChange(
    network::mojom::ConnectionType type) {}

void FakeSyncScheduler::OnThrottled(const base::TimeDelta& throttle_duration) {}

void FakeSyncScheduler::OnTypesThrottled(
    ModelTypeSet types,
    const base::TimeDelta& throttle_duration) {}

void FakeSyncScheduler::OnTypesBackedOff(ModelTypeSet types) {}

bool FakeSyncScheduler::IsAnyThrottleOrBackoff() {
  return false;
}

void FakeSyncScheduler::OnReceivedPollIntervalUpdate(
    const base::TimeDelta& new_interval) {}

void FakeSyncScheduler::OnReceivedCustomNudgeDelays(
    const std::map<ModelType, base::TimeDelta>& nudge_delays) {}

void FakeSyncScheduler::OnReceivedClientInvalidationHintBufferSize(int size) {}

void FakeSyncScheduler::OnSyncProtocolError(const SyncProtocolError& error) {}

void FakeSyncScheduler::OnReceivedGuRetryDelay(const base::TimeDelta& delay) {}

void FakeSyncScheduler::OnReceivedMigrationRequest(ModelTypeSet types) {}

}  // namespace syncer
