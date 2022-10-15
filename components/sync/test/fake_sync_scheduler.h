// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_SCHEDULER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_SCHEDULER_H_

#include <map>
#include <memory>

#include "components/sync/engine/sync_scheduler.h"

namespace syncer {

// A fake implementation of the SyncScheduler. If needed, we should add default
// logic needed for tests (invoking callbacks, etc) here rather than in higher
// level test classes.
class FakeSyncScheduler : public SyncScheduler {
 public:
  FakeSyncScheduler();
  ~FakeSyncScheduler() override;

  void Start(Mode mode, base::Time last_poll_time) override;
  void Stop() override;
  void ScheduleLocalNudge(ModelType type) override;
  void ScheduleLocalRefreshRequest(ModelTypeSet types) override;
  void ScheduleInvalidationNudge(ModelType type) override;
  void SetHasPendingInvalidations(ModelType type,
                                  bool has_invalidation) override;
  void ScheduleConfiguration(sync_pb::SyncEnums::GetUpdatesOrigin origin,
                             ModelTypeSet types_to_download,
                             base::OnceClosure ready_task) override;

  void ScheduleInitialSyncNudge(ModelType model_type) override;
  void SetNotificationsEnabled(bool notifications_enabled) override;

  void OnCredentialsUpdated() override;
  void OnConnectionStatusChange(network::mojom::ConnectionType type) override;

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override;
  void OnTypesThrottled(ModelTypeSet types,
                        const base::TimeDelta& throttle_duration) override;
  void OnTypesBackedOff(ModelTypeSet types) override;
  bool IsAnyThrottleOrBackoff() override;
  void OnReceivedPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  void OnReceivedCustomNudgeDelays(
      const std::map<ModelType, base::TimeDelta>& nudge_delays) override;
  void OnSyncProtocolError(const SyncProtocolError& error) override;
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override;
  void OnReceivedMigrationRequest(ModelTypeSet types) override;
  void OnReceivedQuotaParamsForExtensionTypes(
      absl::optional<int> max_tokens,
      absl::optional<base::TimeDelta> refill_interval,
      absl::optional<base::TimeDelta> depleted_quota_nudge_delay) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_SCHEDULER_H_
