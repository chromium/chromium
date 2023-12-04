// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_scheduler_impl.h"

#include <cstring>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/backoff_delay_provider.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/protocol/sync_enums.pb.h"

using base::TimeTicks;

namespace syncer {

namespace {

constexpr base::TimeDelta kLocalChangeNudgeDelayForTest = base::Milliseconds(1);

bool IsConfigRelatedUpdateOriginValue(
    sync_pb::SyncEnums::GetUpdatesOrigin origin) {
  switch (origin) {
    case sync_pb::SyncEnums::RECONFIGURATION:
    case sync_pb::SyncEnums::MIGRATION:
    case sync_pb::SyncEnums::NEW_CLIENT:
    case sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE:
    case sync_pb::SyncEnums::PROGRAMMATIC:
      return true;
    case sync_pb::SyncEnums::UNKNOWN_ORIGIN:
    case sync_pb::SyncEnums::PERIODIC:
    case sync_pb::SyncEnums::GU_TRIGGER:
    case sync_pb::SyncEnums::RETRY:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ShouldRequestEarlyExit(const SyncProtocolError& error) {
  switch (error.error_type) {
    case SYNC_SUCCESS:
    case MIGRATION_DONE:
    case THROTTLED:
    case TRANSIENT_ERROR:
    case PARTIAL_FAILURE:
      return false;
    case NOT_MY_BIRTHDAY:
    case CLIENT_DATA_OBSOLETE:
    case DISABLED_BY_ADMIN:
    case ENCRYPTION_OBSOLETE:
      // If we send terminate sync early then |sync_cycle_ended| notification
      // would not be sent. If there were no actions then |ACTIONABLE_ERROR|
      // notification wouldnt be sent either. Then the UI layer would be left
      // waiting forever. So assert we would send something.
      DCHECK_NE(error.action, UNKNOWN_ACTION);
      return true;
    case UNKNOWN_ERROR:
      // TODO(crbug.com/1081266): This NOTREACHED is questionable because the
      // sync server can cause it.
      NOTREACHED();
      return false;
    case CONFLICT:
    case INVALID_MESSAGE:
      NOTREACHED();
      return false;
  }
  return false;
}

bool IsActionableProtocolError(const SyncProtocolError& error) {
  return (error.action != UNKNOWN_ACTION);
}

}  // namespace

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

SyncSchedulerImpl::SyncSchedulerImpl(
    const std::string& name,
    std::unique_ptr<BackoffDelayProvider> delay_provider,
    SyncCycleContext* context,
    std::unique_ptr<Syncer> syncer,
    bool ignore_auth_credentials,
    bool sync_poll_immediately_on_every_startup)
    : name_(name),
      syncer_poll_interval_(context->poll_interval()),
      delay_provider_(std::move(delay_provider)),
      syncer_(std::move(syncer)),
      cycle_context_(context),
      ignore_auth_credentials_(ignore_auth_credentials),
      sync_poll_immediately_on_every_startup_(
          sync_poll_immediately_on_every_startup) {}

SyncSchedulerImpl::~SyncSchedulerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
}

void SyncSchedulerImpl::OnCredentialsUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this is the first time we got credentials, or we were previously in an
  // auth error state, then try connecting to the server now.
  HttpResponse::ServerConnectionCode server_status =
      cycle_context_->connection_manager()->server_status();
  if (server_status == HttpResponse::NONE ||
      server_status == HttpResponse::SYNC_AUTH_ERROR) {
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnConnectionStatusChange(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (type != network::mojom::ConnectionType::CONNECTION_NONE &&
      HttpResponse::CONNECTION_UNAVAILABLE ==
          cycle_context_->connection_manager()->server_status()) {
    // Optimistically assume that the connection is fixed and try
    // connecting.
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnServerConnectionErrorFixed() {
  // There could be a pending nudge or configuration job in several cases:
  //
  // 1. We're in exponential backoff.
  // 2. We're silenced / throttled.
  // 3. A nudge was saved previously due to not having a valid access token.
  // 4. A nudge was scheduled + saved while in configuration mode.
  //
  // In all cases except (2), we want to retry contacting the server. We use
  // CANARY_PRIORITY to achieve this, and note that nothing -- not even a
  // canary job -- can bypass a THROTTLED WaitInterval. The only thing that
  // has the authority to do that is the Unthrottle timer.
  TrySyncCycleJob(CANARY_PRIORITY);
}

void SyncSchedulerImpl::Start(Mode mode, base::Time last_poll_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string thread_name = base::PlatformThread::GetName();
  if (thread_name.empty())
    thread_name = "<Main thread>";
  SDVLOG(2) << "Start called from thread " << thread_name << " with mode "
            << GetModeString(mode);
  if (!started_) {
    started_ = true;
    SendInitialSnapshot();
  }

  Mode old_mode = mode_;
  mode_ = mode;
  base::Time now = base::Time::Now();

  // Only adjust the poll reset time if the last poll is valid and in the past.
  if (!last_poll_time.is_null() && last_poll_time <= now) {
    base::Time adjusted_last_poll_time =
        ComputeLastPollOnStart(last_poll_time, GetPollInterval(), now,
                               sync_poll_immediately_on_every_startup_);
    // Convert from base::Time (used for persisting) to base::TimeTicks (used
    // for scheduling). Note that TimeTicks values are not safe to persist, and
    // may "pause" while the device is suspended.
    base::TimeDelta time_since_last_poll = now - adjusted_last_poll_time;
    last_poll_reset_ = TimeTicks::Now() - time_since_last_poll;
  }

  if (old_mode != mode_ && mode_ == NORMAL_MODE) {
    // We just got back to normal mode.  Let's try to run the work that was
    // queued up while we were configuring.

    AdjustPolling(UPDATE_INTERVAL);  // Will kick start poll timer if needed.

    // Update our current time before checking IsRetryRequired().
    nudge_tracker_.SetSyncCycleStartTime(TimeTicks::Now());
    if (nudge_tracker_.IsSyncRequired(GetEnabledAndUnblockedTypes()) &&
        CanRunNudgeJobNow(NORMAL_PRIORITY)) {
      TrySyncCycleJob(NORMAL_PRIORITY);
    }
  }
}

// static
base::Time SyncSchedulerImpl::ComputeLastPollOnStart(
    base::Time last_poll,
    base::TimeDelta poll_interval,
    base::Time now,
    bool sync_poll_immediately_on_every_startup) {
  if (sync_poll_immediately_on_every_startup) {
    // Hack: Pretend the last poll happened sufficiently long ago to trigger a
    // poll.
    return now - (poll_interval + base::Seconds(1));
  }
  return last_poll;
}

ModelTypeSet SyncSchedulerImpl::GetEnabledAndUnblockedTypes() {
  ModelTypeSet enabled_types = cycle_context_->GetConnectedTypes();
  ModelTypeSet enabled_protocol_types =
      Intersection(ProtocolTypes(), enabled_types);
  ModelTypeSet blocked_types = nudge_tracker_.GetBlockedTypes();
  return Difference(enabled_protocol_types, blocked_types);
}

void SyncSchedulerImpl::SetHasPendingInvalidations(ModelType type,
                                                   bool has_invalidation) {
  nudge_tracker_.SetHasPendingInvalidations(type, has_invalidation);
}

void SyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SyncCycleEvent event(SyncCycleEvent::STATUS_CHANGED);
  event.snapshot = SyncCycle(cycle_context_, this).TakeSnapshot();
  for (SyncEngineEventListener& observer : *cycle_context_->listeners())
    observer.OnSyncCycleEvent(event);
}

void SyncSchedulerImpl::ScheduleConfiguration(
    sync_pb::SyncEnums::GetUpdatesOrigin origin,
    ModelTypeSet types_to_download,
    base::OnceClosure ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsConfigRelatedUpdateOriginValue(origin));
  DCHECK_EQ(CONFIGURATION_MODE, mode_);
  DCHECK(!ready_task.is_null());
  DCHECK(started_) << "Scheduler must be running to configure.";
  SDVLOG(2) << "Reconfiguring syncer.";

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!pending_configure_params_);

  // Only reconfigure if we have types to download.
  if (!types_to_download.Empty()) {
    // Cache configuration parameters since TrySyncCycleJob() posts a task.
    pending_configure_params_ = std::make_unique<ConfigurationParams>(
        origin, types_to_download, std::move(ready_task));
    TrySyncCycleJob(NORMAL_PRIORITY);
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    std::move(ready_task).Run();
  }
}

bool SyncSchedulerImpl::CanRunJobNow(JobPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsGlobalThrottle()) {
    SDVLOG(1) << "Unable to run a job because we're throttled.";
    return false;
  }

  if (IsGlobalBackoff() && priority != CANARY_PRIORITY) {
    SDVLOG(1) << "Unable to run a job because we're backing off.";
    return false;
  }

  if (!ignore_auth_credentials_ &&
      cycle_context_->connection_manager()->HasInvalidAccessToken()) {
    SDVLOG(1) << "Unable to run a job because we have no valid access token.";
    return false;
  }

  return true;
}

bool SyncSchedulerImpl::CanRunNudgeJobNow(JobPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanRunJobNow(priority)) {
    SDVLOG(1) << "Unable to run a nudge job right now";
    return false;
  }

  const ModelTypeSet enabled_types = cycle_context_->GetConnectedTypes();
  if (nudge_tracker_.GetBlockedTypes().HasAll(enabled_types)) {
    SDVLOG(1) << "Not running a nudge because we're fully type throttled or "
                 "backed off.";
    return false;
  }

  if (mode_ != NORMAL_MODE) {
    SDVLOG(1) << "Not running nudge because we're not in normal mode.";
    return false;
  }

  return true;
}

void SyncSchedulerImpl::ScheduleLocalNudge(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SDVLOG(2) << "Scheduling sync because of local change to "
            << ModelTypeToDebugString(type);
  base::TimeDelta nudge_delay = nudge_tracker_.RecordLocalChange(type);
  ScheduleNudgeImpl(nudge_delay);
}

void SyncSchedulerImpl::ScheduleLocalRefreshRequest(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!types.Empty());

  SDVLOG(2) << "Scheduling sync because of local refresh request for "
            << ModelTypeSetToDebugString(types);
  base::TimeDelta nudge_delay = nudge_tracker_.RecordLocalRefreshRequest(types);
  ScheduleNudgeImpl(nudge_delay);
}

void SyncSchedulerImpl::ScheduleInvalidationNudge(ModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncer_->IsSyncing());

  SDVLOG(2) << "Scheduling sync because we received invalidation for "
            << ModelTypeToDebugString(model_type);
  base::TimeDelta nudge_delay =
      nudge_tracker_.GetRemoteInvalidationDelay(model_type);
  ScheduleNudgeImpl(nudge_delay);
}

void SyncSchedulerImpl::ScheduleInitialSyncNudge(ModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncer_->IsSyncing());

  SDVLOG(2) << "Scheduling non-blocking initial sync for "
            << ModelTypeToDebugString(model_type);
  nudge_tracker_.RecordInitialSyncRequired(model_type);
  ScheduleNudgeImpl(base::Seconds(0));
}

// TODO(zea): Consider adding separate throttling/backoff for datatype
// refresh requests.
void SyncSchedulerImpl::ScheduleNudgeImpl(const base::TimeDelta& delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!started_) {
    SDVLOG(2) << "Dropping nudge, scheduler is not running.";
    return;
  }

  SDVLOG(2) << "In ScheduleNudgeImpl with delay " << delay.InMilliseconds()
            << " ms";

  if (!CanRunNudgeJobNow(NORMAL_PRIORITY))
    return;

  if (!IsEarlierThanCurrentPendingJob(delay)) {
    // Old job arrives sooner than this one.  Don't reschedule it.
    return;
  }

  // Either there is no existing nudge in flight or the incoming nudge should be
  // made to arrive first (preempt) the existing nudge.  We reschedule in either
  // case.
  SDVLOG(2) << "Scheduling a nudge with " << delay.InMilliseconds()
            << " ms delay";
  pending_wakeup_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&SyncSchedulerImpl::PerformDelayedNudge,
                     weak_ptr_factory_.GetWeakPtr()));
}

const char* SyncSchedulerImpl::GetModeString(SyncScheduler::Mode mode) {
  switch (mode) {
    case CONFIGURATION_MODE:
      return "CONFIGURATION_MODE";
    case NORMAL_MODE:
      return "NORMAL_MODE";
  }
  NOTREACHED();
  return "";
}

void SyncSchedulerImpl::ForceShortNudgeDelayForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (ModelType type : ModelTypeSet::All()) {
    nudge_tracker_.SetLocalChangeDelayIgnoringMinForTest(
        type, kLocalChangeNudgeDelayForTest);
  }
  // We should prevent further changing of nudge delays so if we use real server
  // for integration test then server is not able to increase delays.
  force_short_nudge_delay_for_test_ = true;
}

SyncSchedulerImpl::ConfigurationParams::ConfigurationParams(
    sync_pb::SyncEnums::GetUpdatesOrigin origin,
    ModelTypeSet types_to_download,
    base::OnceClosure ready)
    : origin(origin),
      types_to_download(types_to_download),
      ready_task(std::move(ready)) {
  DCHECK(!ready_task.is_null());
}

SyncSchedulerImpl::ConfigurationParams::~ConfigurationParams() = default;

void SyncSchedulerImpl::DoNudgeSyncCycleJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ModelTypeSet types = GetEnabledAndUnblockedTypes();
  DVLOG(2) << "Will run normal mode sync cycle with types "
           << ModelTypeSetToDebugString(types);
  SyncCycle cycle(cycle_context_, this);
  bool success = syncer_->NormalSyncShare(types, &nudge_tracker_, &cycle);

  if (success) {
    // That cycle took care of any outstanding work we had.
    SDVLOG(2) << "Nudge succeeded.";
    // Note that some types might have become blocked (throttled) during the
    // cycle. NudgeTracker knows of that, and won't clear any "outstanding work"
    // flags for these types.
    nudge_tracker_.RecordSuccessfulSyncCycleIfNotBlocked(types);
    HandleSuccess();

    // If this was a canary, we may need to restart the poll timer (the poll
    // timer may have fired while the scheduler was in an error state, ignoring
    // the poll).
    if (!poll_timer_.IsRunning()) {
      SDVLOG(1) << "Canary succeeded, restarting polling.";
      AdjustPolling(UPDATE_INTERVAL);
    }
  } else {
    HandleFailure(cycle.status_controller().model_neutral_state());
  }
}

void SyncSchedulerImpl::DoConfigurationSyncCycleJob(JobPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(mode_, CONFIGURATION_MODE);
  DCHECK(pending_configure_params_ != nullptr);

  if (!CanRunJobNow(priority)) {
    SDVLOG(2) << "Unable to run configure job right now.";
    return;
  }

  SDVLOG(2) << "Will run configure SyncShare with types "
            << ModelTypeSetToDebugString(
                   pending_configure_params_->types_to_download);
  SyncCycle cycle(cycle_context_, this);
  bool success =
      syncer_->ConfigureSyncShare(pending_configure_params_->types_to_download,
                                  pending_configure_params_->origin, &cycle);

  if (success) {
    SDVLOG(2) << "Configure succeeded.";
    // At this point, the initial sync for the affected types has been
    // completed. Let the nudge tracker know to avoid any spurious extra
    // requests; see also crbug.com/926184.
    nudge_tracker_.RecordInitialSyncDone(
        pending_configure_params_->types_to_download);
    std::move(pending_configure_params_->ready_task).Run();
    pending_configure_params_.reset();
    HandleSuccess();
  } else {
    HandleFailure(cycle.status_controller().model_neutral_state());
    // Sync cycle might receive response from server that causes scheduler to
    // stop and draws pending_configure_params_ invalid.
  }
}

void SyncSchedulerImpl::HandleSuccess() {
  // If we're here, then we successfully reached the server. End all global
  // throttle or backoff.
  wait_interval_.reset();
}

void SyncSchedulerImpl::HandleFailure(
    const ModelNeutralState& model_neutral_state) {
  if (IsGlobalThrottle()) {
    SDVLOG(2) << "Was throttled during previous sync cycle.";
  } else {
    // TODO(skym): Slightly bizarre, the initial SYNC_AUTH_ERROR seems to
    // trigger exponential backoff here, although it's immediately retried with
    // correct credentials, it'd be nice if things were a bit more clean.
    base::TimeDelta previous_delay =
        IsGlobalBackoff()
            ? wait_interval_->length
            : delay_provider_->GetInitialDelay(model_neutral_state);
    base::TimeDelta next_delay = delay_provider_->GetDelay(previous_delay);
    wait_interval_ = std::make_unique<WaitInterval>(
        WaitInterval::BlockingMode::kExponentialBackoff, next_delay);
    SDVLOG(2) << "Sync cycle failed.  Will back off for "
              << wait_interval_->length.InMilliseconds() << "ms.";
  }
}

void SyncSchedulerImpl::DoPollSyncCycleJob() {
  SDVLOG(2) << "Polling with types "
            << ModelTypeSetToDebugString(GetEnabledAndUnblockedTypes());
  SyncCycle cycle(cycle_context_, this);
  bool success = syncer_->PollSyncShare(GetEnabledAndUnblockedTypes(), &cycle);

  // Only restart the timer if the poll succeeded. Otherwise rely on normal
  // failure handling to retry with backoff.
  if (success) {
    AdjustPolling(FORCE_RESET);
    HandleSuccess();
  } else {
    HandleFailure(cycle.status_controller().model_neutral_state());
  }
}

base::TimeDelta SyncSchedulerImpl::GetPollInterval() {
  return syncer_poll_interval_;
}

void SyncSchedulerImpl::AdjustPolling(PollAdjustType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!started_)
    return;

  const base::TimeTicks now = base::TimeTicks::Now();

  switch (type) {
    case UPDATE_INTERVAL:
      if (last_poll_reset_.is_null()) {
        // There was no previous poll. Treat this as if a poll had just been
        // completed.
        last_poll_reset_ = now;
      }
      break;
    case FORCE_RESET:
      // Just restart the timer.
      last_poll_reset_ = now;
      break;
  }

  const base::TimeTicks new_poll_time = last_poll_reset_ + GetPollInterval();
  base::TimeDelta poll_delay = new_poll_time - now;

  if (poll_delay.is_negative()) {
    // The desired poll time was in the past, so trigger a poll now (the
    // timer will post the task asynchronously, so re-entrancy isn't an
    // issue).
    poll_delay = base::TimeDelta();
  }

  SDVLOG(1) << "Scheduling a poll in " << poll_delay.InMinutes() << " minutes.";

  // Adjust poll rate. Start will reset the timer if it was already running.
  poll_timer_.Start(FROM_HERE, poll_delay, this,
                    &SyncSchedulerImpl::PollTimerCallback);
}

void SyncSchedulerImpl::RestartWaiting() {
  NotifyBlockedTypesChanged();
  if (wait_interval_) {
    // Global throttling or backoff.
    if (!IsEarlierThanCurrentPendingJob(wait_interval_->length)) {
      // We check here because if we do not check here, and we already scheduled
      // a global unblock job, we will schedule another unblock job which has
      // same waiting time, then the job will be run later than expected. Even
      // we did not schedule an unblock job when code reach here, it is ok since
      // |TrySyncCycleJobImpl| will call this function after the scheduled job
      // got run.
      return;
    }
    NotifyRetryTime(base::Time::Now() + wait_interval_->length);
    SDVLOG(2) << "Starting WaitInterval timer of length "
              << wait_interval_->length.InMilliseconds() << "ms.";
    if (wait_interval_->mode == WaitInterval::BlockingMode::kThrottled) {
      pending_wakeup_timer_.Start(
          FROM_HERE, wait_interval_->length,
          base::BindOnce(&SyncSchedulerImpl::Unthrottle,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      pending_wakeup_timer_.Start(
          FROM_HERE, wait_interval_->length,
          base::BindOnce(&SyncSchedulerImpl::ExponentialBackoffRetry,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else if (nudge_tracker_.IsAnyTypeBlocked()) {
    // Per-datatype throttled or backed off.
    base::TimeDelta time_until_next_unblock =
        nudge_tracker_.GetTimeUntilNextUnblock();
    if (!IsEarlierThanCurrentPendingJob(time_until_next_unblock)) {
      return;
    }
    NotifyRetryTime(base::Time::Now() + time_until_next_unblock);
    pending_wakeup_timer_.Start(
        FROM_HERE, time_until_next_unblock,
        base::BindOnce(&SyncSchedulerImpl::OnTypesUnblocked,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    NotifyRetryTime(base::Time());
  }
}

void SyncSchedulerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SDVLOG(2) << "Stop called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
  poll_timer_.Stop();
  pending_wakeup_timer_.Stop();
  pending_configure_params_.reset();
  if (started_)
    started_ = false;
}

void SyncSchedulerImpl::TrySyncCycleJob(JobPriority priority) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SyncSchedulerImpl::TrySyncCycleJobImpl,
                                weak_ptr_factory_.GetWeakPtr(), priority));
}

void SyncSchedulerImpl::TrySyncCycleJobImpl(JobPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  nudge_tracker_.SetSyncCycleStartTime(TimeTicks::Now());

  if (mode_ == CONFIGURATION_MODE) {
    if (pending_configure_params_) {
      SDVLOG(2) << "Found pending configure job";
      DoConfigurationSyncCycleJob(priority);
    }
  } else if (CanRunNudgeJobNow(priority)) {
    if (nudge_tracker_.IsSyncRequired(GetEnabledAndUnblockedTypes())) {
      SDVLOG(2) << "Found pending nudge job";
      DoNudgeSyncCycleJob();
    } else if (((TimeTicks::Now() - last_poll_reset_) >= GetPollInterval())) {
      // TODO(crbug.com/1497926): When using a WallClockTimer to schedule
      // polling, use Time instead of TimeTicks for the above computation for
      // consistency.
      SDVLOG(2) << "Found pending poll";
      DoPollSyncCycleJob();
    }
  } else {
    // We must be in an error state. Transitioning out of each of these
    // error states should trigger a canary job.
    DCHECK(IsGlobalThrottle() || IsGlobalBackoff() ||
           cycle_context_->connection_manager()->HasInvalidAccessToken());
  }

  RestartWaiting();
}

void SyncSchedulerImpl::PollTimerCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncer_->IsSyncing());

  TrySyncCycleJob(NORMAL_PRIORITY);
}

void SyncSchedulerImpl::RetryTimerCallback() {
  TrySyncCycleJob(NORMAL_PRIORITY);
}

void SyncSchedulerImpl::Unthrottle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(WaitInterval::BlockingMode::kThrottled, wait_interval_->mode);

  // We're no longer throttled, so clear the wait interval.
  wait_interval_.reset();

  // We treat this as a 'canary' in the sense that it was originally scheduled
  // to run some time ago, failed, and we now want to retry, versus a job that
  // was just created (e.g via ScheduleNudgeImpl). The main implication is
  // that we're careful to update routing info (etc) with such potentially
  // stale canary jobs.
  TrySyncCycleJob(CANARY_PRIORITY);
}

void SyncSchedulerImpl::OnTypesUnblocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  nudge_tracker_.UpdateTypeThrottlingAndBackoffState();

  // Maybe this is a good time to run a nudge job.  Let's try it.
  // If not a good time, reschedule a new run.
  if (nudge_tracker_.IsSyncRequired(GetEnabledAndUnblockedTypes()) &&
      CanRunNudgeJobNow(NORMAL_PRIORITY)) {
    TrySyncCycleJob(NORMAL_PRIORITY);
  } else {
    RestartWaiting();
  }
}

void SyncSchedulerImpl::PerformDelayedNudge() {
  // Circumstances may have changed since we scheduled this delayed nudge.
  // We must check to see if it's OK to run the job before we do so.
  if (CanRunNudgeJobNow(NORMAL_PRIORITY)) {
    TrySyncCycleJob(NORMAL_PRIORITY);
  } else {
    // If we set |wait_interval_| while this PerformDelayedNudge was pending
    // callback scheduled to |retry_timer_|, it's possible we didn't re-schedule
    // because this PerformDelayedNudge was going to execute sooner. If that's
    // the case, we need to make sure we setup to waiting callback now.
    RestartWaiting();
  }
}

void SyncSchedulerImpl::ExponentialBackoffRetry() {
  TrySyncCycleJob(CANARY_PRIORITY);
}

void SyncSchedulerImpl::NotifyRetryTime(base::Time retry_time) {
  for (SyncEngineEventListener& observer : *cycle_context_->listeners())
    observer.OnRetryTimeChanged(retry_time);
}

void SyncSchedulerImpl::NotifyBlockedTypesChanged() {
  ModelTypeSet types = nudge_tracker_.GetBlockedTypes();
  ModelTypeSet throttled_types;
  ModelTypeSet backed_off_types;
  for (ModelType type : types) {
    WaitInterval::BlockingMode mode = nudge_tracker_.GetTypeBlockingMode(type);
    if (mode == WaitInterval::BlockingMode::kThrottled) {
      throttled_types.Put(type);
    } else if (mode == WaitInterval::BlockingMode::kExponentialBackoff ||
               mode ==
                   WaitInterval::BlockingMode::kExponentialBackoffRetrying) {
      backed_off_types.Put(type);
    }
  }

  for (SyncEngineEventListener& observer : *cycle_context_->listeners()) {
    observer.OnThrottledTypesChanged(IsGlobalThrottle() ? ModelTypeSet::All()
                                                        : throttled_types);
    observer.OnBackedOffTypesChanged(IsGlobalBackoff() ? ModelTypeSet::All()
                                                       : backed_off_types);
  }
}

bool SyncSchedulerImpl::IsGlobalThrottle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wait_interval_ &&
         wait_interval_->mode == WaitInterval::BlockingMode::kThrottled;
}

bool SyncSchedulerImpl::IsGlobalBackoff() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wait_interval_ && wait_interval_->mode ==
                               WaitInterval::BlockingMode::kExponentialBackoff;
}

void SyncSchedulerImpl::OnThrottled(const base::TimeDelta& throttle_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean("Sync.ThrottledAllModelTypes", true);
  wait_interval_ = std::make_unique<WaitInterval>(
      WaitInterval::BlockingMode::kThrottled, throttle_duration);
  for (SyncEngineEventListener& observer : *cycle_context_->listeners()) {
    observer.OnThrottledTypesChanged(ModelTypeSet::All());
  }
  RestartWaiting();
}

void SyncSchedulerImpl::OnTypesThrottled(
    ModelTypeSet types,
    const base::TimeDelta& throttle_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SDVLOG(1) << "Throttling " << ModelTypeSetToDebugString(types) << " for "
            << throttle_duration.InSeconds() << " seconds.";
  for (ModelType type : types) {
    base::UmaHistogramEnumeration("Sync.ThrottledSomeModelTypes",
                                  ModelTypeHistogramValue(type));
  }
  nudge_tracker_.SetTypesThrottledUntil(types, throttle_duration,
                                        TimeTicks::Now());
  RestartWaiting();
}

void SyncSchedulerImpl::OnTypesBackedOff(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (ModelType type : types) {
    base::UmaHistogramEnumeration("Sync.BackedOffModelType",
                                  ModelTypeHistogramValue(type));
    base::TimeDelta last_backoff_time = kInitialBackoffRetryTime;
    if (nudge_tracker_.GetTypeBlockingMode(type) ==
        WaitInterval::BlockingMode::kExponentialBackoffRetrying) {
      last_backoff_time = nudge_tracker_.GetTypeLastBackoffInterval(type);
    }

    base::TimeDelta length = delay_provider_->GetDelay(last_backoff_time);
    nudge_tracker_.SetTypeBackedOff(type, length, TimeTicks::Now());
    SDVLOG(1) << "Backing off " << ModelTypeToDebugString(type) << " for "
              << length.InSeconds() << " second.";
  }
  RestartWaiting();
}

bool SyncSchedulerImpl::IsAnyThrottleOrBackoff() {
  return wait_interval_ || nudge_tracker_.IsAnyTypeBlocked();
}

void SyncSchedulerImpl::OnReceivedPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_interval == syncer_poll_interval_) {
    return;
  }
  SDVLOG(1) << "Updating poll interval to " << new_interval.InMinutes()
            << " minutes.";
  syncer_poll_interval_ = new_interval;
  AdjustPolling(UPDATE_INTERVAL);
}

void SyncSchedulerImpl::OnReceivedCustomNudgeDelays(
    const std::map<ModelType, base::TimeDelta>& nudge_delays) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (force_short_nudge_delay_for_test_)
    return;

  for (const auto& [type, delay] : nudge_delays) {
    nudge_tracker_.UpdateLocalChangeDelay(type, delay);
  }
}

void SyncSchedulerImpl::OnSyncProtocolError(
    const SyncProtocolError& sync_protocol_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldRequestEarlyExit(sync_protocol_error)) {
    SDVLOG(2) << "Sync Scheduler requesting early exit.";
    Stop();
  }
  if (IsActionableProtocolError(sync_protocol_error)) {
    SDVLOG(2) << "OnActionableProtocolError";
    for (SyncEngineEventListener& observer : *cycle_context_->listeners())
      observer.OnActionableProtocolError(sync_protocol_error);
  }
}

void SyncSchedulerImpl::OnReceivedGuRetryDelay(const base::TimeDelta& delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  nudge_tracker_.SetNextRetryTime(TimeTicks::Now() + delay);
  retry_timer_.Start(FROM_HERE, delay, this,
                     &SyncSchedulerImpl::RetryTimerCallback);
}

void SyncSchedulerImpl::OnReceivedMigrationRequest(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (SyncEngineEventListener& observer : *cycle_context_->listeners())
    observer.OnMigrationRequested(types);
}

void SyncSchedulerImpl::OnReceivedQuotaParamsForExtensionTypes(
    absl::optional<int> max_tokens,
    absl::optional<base::TimeDelta> refill_interval,
    absl::optional<base::TimeDelta> depleted_quota_nudge_delay) {
  nudge_tracker_.SetQuotaParamsForExtensionTypes(max_tokens, refill_interval,
                                                 depleted_quota_nudge_delay);
}

void SyncSchedulerImpl::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cycle_context_->set_notifications_enabled(notifications_enabled);
  if (notifications_enabled)
    nudge_tracker_.OnInvalidationsEnabled();
  else
    nudge_tracker_.OnInvalidationsDisabled();
}

bool SyncSchedulerImpl::IsEarlierThanCurrentPendingJob(
    const base::TimeDelta& delay) {
  TimeTicks incoming_run_time = TimeTicks::Now() + delay;
  if (pending_wakeup_timer_.IsRunning() &&
      (pending_wakeup_timer_.desired_run_time() < incoming_run_time)) {
    // Old job arrives sooner than this one.
    return false;
  }
  return true;
}

#undef SDVLOG

}  // namespace syncer
