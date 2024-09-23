// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "base/types/strong_alias.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_scheduler.h"
#include "components/sync/engine/syncer.h"

namespace syncer {

class BackoffDelayProvider;
struct ModelNeutralState;

// Lives on the sync sequence.
class SyncSchedulerImpl : public SyncScheduler {
 public:
  // |name| is a display string to identify the sync sequence.
  SyncSchedulerImpl(const std::string& name,
                    std::unique_ptr<BackoffDelayProvider> delay_provider,
                    SyncCycleContext* context,
                    std::unique_ptr<Syncer> syncer,
                    bool ignore_auth_credentials);

  SyncSchedulerImpl(const SyncSchedulerImpl&) = delete;
  SyncSchedulerImpl& operator=(const SyncSchedulerImpl&) = delete;

  // Calls Stop().
  ~SyncSchedulerImpl() override;

  void Start(Mode mode, base::Time last_poll_time) override;
  void ScheduleConfiguration(sync_pb::SyncEnums::GetUpdatesOrigin origin,
                             DataTypeSet types_to_download,
                             base::OnceClosure ready_task) override;
  void Stop() override;
  void ScheduleLocalNudge(DataType type) override;
  void ScheduleLocalRefreshRequest(DataTypeSet types) override;
  void ScheduleInvalidationNudge(DataType type) override;
  void ScheduleInitialSyncNudge(DataType data_type) override;
  void SetNotificationsEnabled(bool notifications_enabled) override;
  void SetHasPendingInvalidations(DataType type,
                                  bool has_invalidations) override;

  void OnCredentialsUpdated() override;
  void OnConnectionStatusChange(network::mojom::ConnectionType type) override;

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override;
  void OnTypesThrottled(DataTypeSet types,
                        const base::TimeDelta& throttle_duration) override;
  void OnTypesBackedOff(DataTypeSet types) override;
  bool IsAnyThrottleOrBackoff() override;
  void OnReceivedPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  void OnReceivedCustomNudgeDelays(
      const std::map<DataType, base::TimeDelta>& nudge_delays) override;
  void OnSyncProtocolError(
      const SyncProtocolError& sync_protocol_error) override;
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override;
  void OnReceivedMigrationRequest(DataTypeSet types) override;
  void OnReceivedQuotaParamsForExtensionTypes(
      std::optional<int> max_tokens,
      std::optional<base::TimeDelta> refill_interval,
      std::optional<base::TimeDelta> depleted_quota_nudge_delay) override;

  bool IsGlobalThrottle() const;
  bool IsGlobalBackoff() const;

  // Reduces nudge delays for all types to a very short value and prevents their
  // further changing by the server. Used to speed up passing of integration
  // tests.
  void ForceShortNudgeDelayForTest();

 private:
  struct ConfigurationParams {
    ConfigurationParams(sync_pb::SyncEnums::GetUpdatesOrigin origin,
                        DataTypeSet types_to_download,
                        base::OnceClosure ready_task);
    ~ConfigurationParams();

    ConfigurationParams(const ConfigurationParams&) = delete;
    ConfigurationParams& operator=(const ConfigurationParams&) = delete;

    const sync_pb::SyncEnums::GetUpdatesOrigin origin;
    const DataTypeSet types_to_download;
    // Callback to invoke on configuration completion.
    base::OnceClosure ready_task;
  };

  // Used as a parameter when triggering sync cycle jobs. Determines whether to
  // respect or ignore any global backoff. (In the usual case where the client
  // is NOT backed off, this makes no difference. It also doesn't affect
  // per-data-type backoff.)
  using RespectGlobalBackoff =
      base::StrongAlias<class RespectGlobalBackoffTag, bool>;

  enum PollAdjustType {
    // Restart the poll interval.
    FORCE_RESET,
    // Restart the poll interval only if its length has changed.
    UPDATE_INTERVAL,
  };

  friend class SyncSchedulerImplTest;

  static const char* GetModeString(Mode mode);

  // Invoke the syncer to perform a nudge job.
  void DoNudgeSyncCycleJob();

  // Invoke the syncer to perform a configuration job.
  void DoConfigurationSyncCycleJob(RespectGlobalBackoff respect_backoff);

  // Helper function for Do{Nudge,Configuration,Poll}SyncCycleJob.
  void HandleSuccess();

  // Helper function for Do{Nudge,Configuration,Poll}SyncCycleJob.
  void HandleFailure(const ModelNeutralState& model_neutral_state);

  // Invoke the Syncer to perform a poll job.
  void DoPollSyncCycleJob();

  // Helper function to calculate poll interval.
  base::TimeDelta GetPollInterval();

  // Adjusts the poll timer to account for new poll interval, and possibly
  // resets the poll interval, depedning on the flag's value.
  void AdjustPolling(PollAdjustType type);

  // Helper to restart pending_wakeup_timer_.
  // This function need to be called in 3 conditions, backoff/throttling
  // happens, unbackoff/unthrottling happens and after |PerformDelayedNudge|
  // runs.
  // This function is for scheduling unbackoff/unthrottling jobs, and the
  // poriority is, global unbackoff/unthrottling job first, if there is no
  //  global backoff/throttling, then try to schedule types
  // unbackoff/unthrottling job.
  void RestartWaiting();

  // Determines if we're allowed to contact the server right now.
  bool CanRunJobNow(RespectGlobalBackoff respect_backoff);

  // Determines if we're allowed to contact the server right now.
  bool CanRunNudgeJobNow(RespectGlobalBackoff respect_backoff);

  // If the scheduler's current state supports it, this will create a job based
  // on the passed in parameters and coalesce it with any other pending jobs,
  // then post a delayed task to run it.  It may also choose to drop the job or
  // save it for later, depending on the scheduler's current state.
  void ScheduleNudgeImpl(const base::TimeDelta& delay);

  // Helper to signal listeners about changed retry time.
  void NotifyRetryTime(base::Time retry_time);

  // Helper to signal listeners about changed throttled or backed off types.
  void NotifyBlockedTypesChanged();

  // Looks for pending work and, if it finds any, runs it. TrySyncCycleJob just
  // posts a call to TrySyncCycleJobImpl on the current sequence.
  void TrySyncCycleJob(RespectGlobalBackoff respect_backoff);
  void TrySyncCycleJobImpl(RespectGlobalBackoff respect_backoff);

  // Transitions out of the THROTTLED WaitInterval then triggers a job which
  // ignores global backoff. This is used for global throttling.
  void Unthrottle();

  // Called when a per-type throttling or backing off interval expires.
  void OnTypesUnblocked();

  // Runs a normal nudge job when the scheduled timer expires.
  void PerformDelayedNudge();

  // Attempts to exit global backoff (BlockingMode::kExponentialBackoff) by
  // triggering a job which ignores global backoff.
  void ExponentialBackoffRetry();

  // Called when the root cause of the current connection error is fixed.
  void OnServerConnectionErrorFixed();

  // Creates a cycle for a poll and performs the sync.
  void PollTimerCallback();

  // Creates a cycle for a retry and performs the sync.
  void RetryTimerCallback();

  // Returns the set of types that are enabled and not currently throttled and
  // backed off.
  DataTypeSet GetEnabledAndUnblockedTypes();

  // Called as we are started to broadcast an initial cycle snapshot
  // containing data like initial_sync_ended.  Important when the client starts
  // up and does not need to perform an initial sync.
  void SendInitialSnapshot();

  bool IsEarlierThanCurrentPendingJob(const base::TimeDelta& delay);

  // Used for logging.
  const std::string name_;

  // Set in Start(), unset in Stop().
  bool started_ = false;

  // The interval between poll requests. Can be updated by the server.
  base::TimeDelta syncer_poll_interval_;

  // Timer for polling. Restarted on each successful poll, and when entering
  // normal sync mode or exiting an error state. Not active in configuration
  // mode.
  // Note that this is a WallClockTimer (as opposed to a regular OneShotTimer)
  // so that it continues counting even if the device is suspended.
  base::WallClockTimer poll_timer_;

  // The mode of operation.
  Mode mode_ = CONFIGURATION_MODE;

  // Current wait state.  Null if we're not in backoff and not throttled.
  std::unique_ptr<WaitInterval> wait_interval_;

  std::unique_ptr<BackoffDelayProvider> delay_provider_;

  // The timer for the next pending task (except for polling, which has its own
  // timer). This can be a delayed nudge (standard case), or throttling/backoff
  // (either global or for some data type(s)).
  // TODO(crbug.com/40939309): Maybe use a WallClockTimer, so that
  // throttling/backoff continue counting even if the device is suspended?
  base::OneShotTimer pending_wakeup_timer_;

  // Storage for variables related to an in-progress configure request.  Note
  // that (mode_ != CONFIGURATION_MODE) \implies !pending_configure_params_.
  std::unique_ptr<ConfigurationParams> pending_configure_params_;

  // Keeps track of work that the syncer needs to handle.
  NudgeTracker nudge_tracker_;

  // Invoked to run through the sync cycle.
  const std::unique_ptr<Syncer> syncer_;

  const raw_ptr<SyncCycleContext> cycle_context_;

  // The time when the last poll request finished. Used for computing the next
  // poll time.
  base::Time last_poll_reset_time_;

  // One-shot timer for scheduling GU retry according to delay set by server.
  base::OneShotTimer retry_timer_;

  // Dictates if the scheduler should wait for authentication to happen or not.
  const bool ignore_auth_credentials_;

  // Used to prevent changing nudge delays by the server in integration tests.
  bool force_short_nudge_delay_for_test_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
