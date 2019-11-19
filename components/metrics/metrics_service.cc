// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Description of the life cycle of a instance of MetricsService.
//
//  OVERVIEW
//
// A MetricsService instance is typically created at application startup.  It is
// the central controller for the acquisition of log data, and the automatic
// transmission of that log data to an external server.  Its major job is to
// manage logs, grouping them for transmission, and transmitting them.  As part
// of its grouping, MS finalizes logs by including some just-in-time gathered
// memory statistics, snapshotting the current stats of numerous histograms,
// closing the logs, translating to protocol buffer format, and compressing the
// results for transmission.  Transmission includes submitting a compressed log
// as data in a URL-post, and retransmitting (or retaining at process
// termination) if the attempted transmission failed.  Retention across process
// terminations is done using the the PrefServices facilities. The retained logs
// (the ones that never got transmitted) are compressed and base64-encoded
// before being persisted.
//
// Logs fall into one of two categories: "initial logs," and "ongoing logs."
// There is at most one initial log sent for each complete run of Chrome (from
// startup, to browser shutdown).  An initial log is generally transmitted some
// short time (1 minute?) after startup, and includes stats such as recent crash
// info, the number and types of plugins, etc.  The external server's response
// to the initial log conceptually tells this MS if it should continue
// transmitting logs (during this session). The server response can actually be
// much more detailed, and always includes (at a minimum) how often additional
// ongoing logs should be sent.
//
// After the above initial log, a series of ongoing logs will be transmitted.
// The first ongoing log actually begins to accumulate information stating when
// the MS was first constructed.  Note that even though the initial log is
// commonly sent a full minute after startup, the initial log does not include
// much in the way of user stats.   The most common interlog period (delay)
// is 30 minutes. That time period starts when the first user action causes a
// logging event.  This means that if there is no user action, there may be long
// periods without any (ongoing) log transmissions.  Ongoing logs typically
// contain very detailed records of user activities (ex: opened tab, closed
// tab, fetched URL, maximized window, etc.)  In addition, just before an
// ongoing log is closed out, a call is made to gather memory statistics.  Those
// memory statistics are deposited into a histogram, and the log finalization
// code is then called.  In the finalization, a call to a Histogram server
// acquires a list of all local histograms that have been flagged for upload
// to the UMA server.  The finalization also acquires the most recent number
// of page loads, along with any counts of renderer or plugin crashes.
//
// When the browser shuts down, there will typically be a fragment of an ongoing
// log that has not yet been transmitted.  At shutdown time, that fragment is
// closed (including snapshotting histograms), and persisted, for potential
// transmission during a future run of the product.
//
// There are two slightly abnormal shutdown conditions.  There is a
// "disconnected scenario," and a "really fast startup and shutdown" scenario.
// In the "never connected" situation, the user has (during the running of the
// process) never established an internet connection.  As a result, attempts to
// transmit the initial log have failed, and a lot(?) of data has accumulated in
// the ongoing log (which didn't yet get closed, because there was never even a
// contemplation of sending it).  There is also a kindred "lost connection"
// situation, where a loss of connection prevented an ongoing log from being
// transmitted, and a (still open) log was stuck accumulating a lot(?) of data,
// while the earlier log retried its transmission.  In both of these
// disconnected situations, two logs need to be, and are, persistently stored
// for future transmission.
//
// The other unusual shutdown condition, termed "really fast startup and
// shutdown," involves the deliberate user termination of the process before
// the initial log is even formed or transmitted. In that situation, no logging
// is done, but the historical crash statistics remain (unlogged) for inclusion
// in a future run's initial log.  (i.e., we don't lose crash stats).
//
// With the above overview, we can now describe the state machine's various
// states, based on the State enum specified in the state_ member.  Those states
// are:
//
//  INITIALIZED,          // Constructor was called.
//  INIT_TASK_SCHEDULED,  // Waiting for deferred init tasks to finish.
//  INIT_TASK_DONE,       // Waiting for timer to send initial log.
//  SENDING_LOGS,         // Sending logs and creating new ones when we run out.
//
// In more detail, we have:
//
//    INITIALIZED,            // Constructor was called.
// The MS has been constructed, but has taken no actions to compose the
// initial log.
//
//    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to finish.
// Typically about 30 seconds after startup, a task is sent to a second thread
// (the file thread) to perform deferred (lower priority and slower)
// initialization steps such as getting the list of plugins.  That task will
// (when complete) make an async callback (via a Task) to indicate the
// completion.
//
//    INIT_TASK_DONE,         // Waiting for timer to send initial log.
// The callback has arrived, and it is now possible for an initial log to be
// created.  This callback typically arrives back less than one second after
// the deferred init task is dispatched.
//
//    SENDING_LOGS,  // Sending logs an creating new ones when we run out.
// Logs from previous sessions have been loaded, and initial logs have been
// created (an optional stability log and the first metrics log).  We will
// send all of these logs, and when run out, we will start cutting new logs
// to send.  We will also cut a new log if we expect a shutdown.
//
// The progression through the above states is simple, and sequential.
// States proceed from INITIAL to SENDING_LOGS, and remain in the latter until
// shutdown.
//
// Also note that whenever we successfully send a log, we mirror the list
// of logs into the PrefService. This ensures that IF we crash, we won't start
// up and retransmit our old logs again.
//
// Due to race conditions, it is always possible that a log file could be sent
// twice.  For example, if a log file is sent, but not yet acknowledged by
// the external server, and the user shuts down, then a copy of the log may be
// saved for re-transmission.  These duplicates could be filtered out server
// side, but are not expected to be a significant problem.
//
//
//------------------------------------------------------------------------------

#include "components/metrics/metrics_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_manager.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/stability_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"

namespace metrics {

namespace {

// The delay, in seconds, after starting recording before doing expensive
// initialization work.
#if defined(OS_ANDROID) || defined(OS_IOS)
// On mobile devices, a significant portion of sessions last less than a minute.
// Use a shorter timer on these platforms to avoid losing data.
// TODO(dfalcantara): To avoid delaying startup, tighten up initialization so
//                    that it occurs after the user gets their initial page.
const int kInitializationDelaySeconds = 5;
#else
const int kInitializationDelaySeconds = 30;
#endif

// The browser last live timestamp is updated every 15 minutes.
const int kUpdateAliveTimestampSeconds = 15 * 60;

#if defined(OS_ANDROID) || defined(OS_IOS)
void MarkAppCleanShutdownAndCommit(CleanExitBeacon* clean_exit_beacon,
                                   PrefService* local_state) {
  clean_exit_beacon->WriteBeaconValue(true);
  // Start writing right away (write happens on a different thread).
  local_state->CommitPendingWrite();
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace

// static
MetricsService::ShutdownCleanliness MetricsService::clean_shutdown_status_ =
    MetricsService::CLEANLY_SHUTDOWN;

// static
void MetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  CleanExitBeacon::RegisterPrefs(registry);
  MetricsStateManager::RegisterPrefs(registry);
  MetricsLog::RegisterPrefs(registry);
  StabilityMetricsProvider::RegisterPrefs(registry);
  MetricsReportingService::RegisterPrefs(registry);

  registry->RegisterIntegerPref(prefs::kMetricsSessionID, -1);

  registry->RegisterInt64Pref(prefs::kUninstallLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kUninstallMetricsUptimeSec, 0);
}

MetricsService::MetricsService(MetricsStateManager* state_manager,
                               MetricsServiceClient* client,
                               PrefService* local_state)
    : reporting_service_(client, local_state),
      histogram_snapshot_manager_(this),
      state_manager_(state_manager),
      client_(client),
      local_state_(local_state),
      recording_state_(UNSET),
      test_mode_active_(false),
      state_(INITIALIZED),
      idle_since_last_transmission_(false),
      session_id_(-1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_manager_);
  DCHECK(client_);
  DCHECK(local_state_);

  RegisterMetricsProvider(
      std::make_unique<StabilityMetricsProvider>(local_state_));

  RegisterMetricsProvider(state_manager_->GetProvider());

  RegisterMetricsProvider(std::make_unique<variations::FieldTrialsProvider>(
      &synthetic_trial_registry_, base::StringPiece()));
}

MetricsService::~MetricsService() {
  DisableRecording();
}

void MetricsService::InitializeMetricsRecordingState() {
  reporting_service_.Initialize();
  InitializeMetricsState();

  base::Closure upload_callback =
      base::Bind(&MetricsService::StartScheduledUpload,
                 self_ptr_factory_.GetWeakPtr());

  rotation_scheduler_.reset(new MetricsRotationScheduler(
      upload_callback,
      // MetricsServiceClient outlives MetricsService, and
      // MetricsRotationScheduler is tied to the lifetime of |this|.
      base::Bind(&MetricsServiceClient::GetUploadInterval,
                 base::Unretained(client_)),
      client_->ShouldStartUpFastForTesting()));

  // Init() has to be called after LogCrash() in order for LogCrash() to work.
  delegating_provider_.Init();
}

void MetricsService::Start() {
  HandleIdleSinceLastTransmission(false);
  EnableRecording();
  EnableReporting();
}

void MetricsService::StartRecordingForTests() {
  test_mode_active_ = true;
  EnableRecording();
  DisableReporting();
}

void MetricsService::StartUpdatingLastLiveTimestamp() {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsService::UpdateLastLiveTimestampTask,
                     self_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kUpdateAliveTimestampSeconds));
}

void MetricsService::Stop() {
  HandleIdleSinceLastTransmission(false);
  DisableReporting();
  DisableRecording();
}

void MetricsService::EnableReporting() {
  if (reporting_service_.reporting_active())
    return;
  reporting_service_.EnableReporting();
  StartSchedulerIfNecessary();
}

void MetricsService::DisableReporting() {
  reporting_service_.DisableReporting();
}

std::string MetricsService::GetClientId() {
  return state_manager_->client_id();
}

int64_t MetricsService::GetInstallDate() {
  return state_manager_->GetInstallDate();
}

bool MetricsService::WasLastShutdownClean() const {
  return state_manager_->clean_exit_beacon()->exited_cleanly();
}

void MetricsService::EnableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (recording_state_ == ACTIVE)
    return;
  recording_state_ = ACTIVE;

  state_manager_->ForceClientIdCreation();
  client_->SetMetricsClientId(state_manager_->client_id());

  if (!log_manager_.current_log())
    OpenNewLog();

  delegating_provider_.OnRecordingEnabled();

  // Fill in the system profile in the log and persist it (to prefs, .pma and
  // crashpad). This includes running the providers so that information like
  // field trials and hardware info is provided. If Chrome crashes before this
  // log is completed, the .pma file will have this system profile.
  RecordCurrentEnvironment(log_manager_.current_log(), /*complete=*/false);

  base::RemoveActionCallback(action_callback_);
  action_callback_ = base::Bind(&MetricsService::OnUserAction,
                                base::Unretained(this));
  base::AddActionCallback(action_callback_);
}

void MetricsService::DisableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (recording_state_ == INACTIVE)
    return;
  recording_state_ = INACTIVE;

  base::RemoveActionCallback(action_callback_);

  delegating_provider_.OnRecordingDisabled();

  PushPendingLogsToPersistentStorage();
}

bool MetricsService::recording_active() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return recording_state_ == ACTIVE;
}

bool MetricsService::reporting_active() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return reporting_service_.reporting_active();
}

bool MetricsService::has_unsent_logs() const {
  return reporting_service_.metrics_log_store()->has_unsent_logs();
}

void MetricsService::RecordDelta(const base::HistogramBase& histogram,
                                 const base::HistogramSamples& snapshot) {
  log_manager_.current_log()->RecordHistogramDelta(histogram.histogram_name(),
                                                   snapshot);
}

void MetricsService::HandleIdleSinceLastTransmission(bool in_idle) {
  // If there wasn't a lot of action, maybe the computer was asleep, in which
  // case, the log transmissions should have stopped.  Here we start them up
  // again.
  if (!in_idle && idle_since_last_transmission_)
    StartSchedulerIfNecessary();
  idle_since_last_transmission_ = in_idle;
}

void MetricsService::OnApplicationNotIdle() {
  if (recording_state_ == ACTIVE)
    HandleIdleSinceLastTransmission(false);
}

void MetricsService::RecordStartOfSessionEnd() {
  LogCleanShutdown(false);
}

void MetricsService::RecordCompletedSessionEnd() {
  LogCleanShutdown(true);
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void MetricsService::OnAppEnterBackground(bool keep_recording_in_background) {
  if (!keep_recording_in_background) {
    rotation_scheduler_->Stop();
    reporting_service_.Stop();
  }

  MarkAppCleanShutdownAndCommit(state_manager_->clean_exit_beacon(),
                                local_state_);

  // Give providers a chance to persist histograms as part of being
  // backgrounded.
  delegating_provider_.OnAppEnterBackground();

  // At this point, there's no way of knowing when the process will be
  // killed, so this has to be treated similar to a shutdown, closing and
  // persisting all logs. Unlinke a shutdown, the state is primed to be ready
  // to continue logging and uploading if the process does return.
  if (recording_active() && state_ >= SENDING_LOGS) {
    PushPendingLogsToPersistentStorage();
    // Persisting logs closes the current log, so start recording a new log
    // immediately to capture any background work that might be done before the
    // process is killed.
    OpenNewLog();
  }
}

void MetricsService::OnAppEnterForeground(bool force_open_new_log) {
  state_manager_->clean_exit_beacon()->WriteBeaconValue(false);
  StartSchedulerIfNecessary();

  if (force_open_new_log && recording_active() && state_ >= SENDING_LOGS) {
    // Because state_ >= SENDING_LOGS, PushPendingLogsToPersistentStorage()
    // will close the log, allowing a new log to be opened.
    PushPendingLogsToPersistentStorage();
    OpenNewLog();
  }
}

#else
void MetricsService::LogNeedForCleanShutdown() {
  state_manager_->clean_exit_beacon()->WriteBeaconValue(false);
  // Redundant setting to be sure we call for a clean shutdown.
  clean_shutdown_status_ = NEED_TO_SHUTDOWN;
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

void MetricsService::RecordBreakpadRegistration(bool success) {
  StabilityMetricsProvider(local_state_).RecordBreakpadRegistration(success);
}

void MetricsService::RecordBreakpadHasDebugger(bool has_debugger) {
  StabilityMetricsProvider(local_state_)
      .RecordBreakpadHasDebugger(has_debugger);
}

void MetricsService::ClearSavedStabilityMetrics() {
  delegating_provider_.ClearSavedStabilityMetrics();
}

void MetricsService::PushExternalLog(const std::string& log) {
  log_store()->StoreLog(log, MetricsLog::ONGOING_LOG);
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Initialization methods

void MetricsService::InitializeMetricsState() {
  const int64_t buildtime = MetricsLog::GetBuildTime();
  const std::string version = client_->GetVersionString();

  bool version_changed = false;
  EnvironmentRecorder recorder(local_state_);
  int64_t previous_buildtime = recorder.GetLastBuildtime();
  std::string previous_version = recorder.GetLastVersion();
  if (previous_buildtime != buildtime || previous_version != version) {
    recorder.SetBuildtimeAndVersion(buildtime, version);
    version_changed = true;
  }

  session_id_ = local_state_->GetInteger(prefs::kMetricsSessionID);

  StabilityMetricsProvider provider(local_state_);
  if (!state_manager_->clean_exit_beacon()->exited_cleanly()) {
    provider.LogCrash(
        state_manager_->clean_exit_beacon()->browser_last_live_timestamp());
    // Reset flag, and wait until we call LogNeedForCleanShutdown() before
    // monitoring.
    state_manager_->clean_exit_beacon()->WriteBeaconValue(true);
  }

  // HasPreviousSessionData is called first to ensure it is never bypassed.
  const bool is_initial_stability_log_required =
      delegating_provider_.HasPreviousSessionData() ||
      !state_manager_->clean_exit_beacon()->exited_cleanly();
  bool has_initial_stability_log = false;
  if (is_initial_stability_log_required) {
    // If the previous session didn't exit cleanly, or if any provider
    // explicitly requests it, prepare an initial stability log -
    // provided UMA is enabled.
    if (state_manager_->IsMetricsReportingEnabled()) {
      has_initial_stability_log = PrepareInitialStabilityLog(previous_version);
      if (!has_initial_stability_log)
        provider.LogStabilityLogDeferred();
    }
  }

  // If the version changed, but no initial stability log was generated, clear
  // the stability stats from the previous version (so that they don't get
  // attributed to the current version). This could otherwise happen due to a
  // number of different edge cases, such as if the last version crashed before
  // it could save off a system profile or if UMA reporting is disabled (which
  // normally results in stats being accumulated).
  if (version_changed && !has_initial_stability_log) {
    ClearSavedStabilityMetrics();
    provider.LogStabilityDataDiscarded();
  }

  // If the version changed, the system profile is obsolete and needs to be
  // cleared. This is to avoid the stability data misattribution that could
  // occur if the current version crashed before saving its own system profile.
  // Note however this clearing occurs only after preparing the initial
  // stability log, an operation that requires the previous version's system
  // profile. At this point, stability metrics pertaining to the previous
  // version have been cleared.
  if (version_changed)
    recorder.ClearEnvironmentFromPrefs();

  // Update session ID.
  ++session_id_;
  local_state_->SetInteger(prefs::kMetricsSessionID, session_id_);

  // Notify stability metrics providers about the launch.
  provider.LogLaunch();
  provider.CheckLastSessionEndCompleted();

  // Call GetUptimes() for the first time, thus allowing all later calls
  // to record incremental uptimes accurately.
  base::TimeDelta ignored_uptime_parameter;
  base::TimeDelta startup_uptime;
  GetUptimes(local_state_, &startup_uptime, &ignored_uptime_parameter);
  DCHECK_EQ(0, startup_uptime.InMicroseconds());

  // Bookkeeping for the uninstall metrics.
  IncrementLongPrefsValue(prefs::kUninstallLaunchCount);
}

void MetricsService::OnUserAction(const std::string& action) {
  log_manager_.current_log()->RecordUserAction(action);
  HandleIdleSinceLastTransmission(false);
}

void MetricsService::FinishedInitTask() {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
  state_ = INIT_TASK_DONE;

  // Create the initial log.
  if (!initial_metrics_log_) {
    initial_metrics_log_ = CreateLog(MetricsLog::ONGOING_LOG);
    delegating_provider_.OnDidCreateMetricsLog();
  }

  rotation_scheduler_->InitTaskComplete();
}

void MetricsService::GetUptimes(PrefService* pref,
                                base::TimeDelta* incremental_uptime,
                                base::TimeDelta* uptime) {
  base::TimeTicks now = base::TimeTicks::Now();
  // If this is the first call, init |first_updated_time_| and
  // |last_updated_time_|.
  if (last_updated_time_.is_null()) {
    first_updated_time_ = now;
    last_updated_time_ = now;
  }
  *incremental_uptime = now - last_updated_time_;
  *uptime = now - first_updated_time_;
  last_updated_time_ = now;

  const int64_t incremental_time_secs = incremental_uptime->InSeconds();
  if (incremental_time_secs > 0) {
    int64_t metrics_uptime = pref->GetInt64(prefs::kUninstallMetricsUptimeSec);
    metrics_uptime += incremental_time_secs;
    pref->SetInt64(prefs::kUninstallMetricsUptimeSec, metrics_uptime);
  }
}

//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::OpenNewLog() {
  DCHECK(!log_manager_.current_log());

  log_manager_.BeginLoggingWithLog(CreateLog(MetricsLog::ONGOING_LOG));
  delegating_provider_.OnDidCreateMetricsLog();
  if (state_ == INITIALIZED) {
    // We only need to schedule that run once.
    state_ = INIT_TASK_SCHEDULED;

    base::TimeDelta initialization_delay = base::TimeDelta::FromSeconds(
        client_->ShouldStartUpFastForTesting() ? 0
                                               : kInitializationDelaySeconds);
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MetricsService::StartInitTask,
                       self_ptr_factory_.GetWeakPtr()),
        initialization_delay);

    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MetricsService::PrepareProviderMetricsTask,
                       self_ptr_factory_.GetWeakPtr()),
        2 * initialization_delay);
  }
}

void MetricsService::StartInitTask() {
  delegating_provider_.AsyncInit(base::Bind(&MetricsService::FinishedInitTask,
                                            self_ptr_factory_.GetWeakPtr()));
}

void MetricsService::CloseCurrentLog() {
  if (!log_manager_.current_log())
    return;

  // If a persistent allocator is in use, update its internal histograms (such
  // as how much memory is being used) before reporting.
  base::PersistentHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (allocator)
    allocator->UpdateTrackingHistograms();

  // Put incremental data (histogram deltas, and realtime stats deltas) at the
  // end of all log transmissions (initial log handles this separately).
  // RecordIncrementalStabilityElements only exists on the derived
  // MetricsLog class.
  MetricsLog* current_log = log_manager_.current_log();
  DCHECK(current_log);
  RecordCurrentEnvironment(current_log, /*complete=*/true);
  base::TimeDelta incremental_uptime;
  base::TimeDelta uptime;
  GetUptimes(local_state_, &incremental_uptime, &uptime);
  current_log->RecordCurrentSessionData(&delegating_provider_,
                                        incremental_uptime, uptime);
  RecordCurrentHistograms();
  current_log->TruncateEvents();
  DVLOG(1) << "Generated an ongoing log.";
  log_manager_.FinishCurrentLog(log_store());
}

void MetricsService::PushPendingLogsToPersistentStorage() {
  if (state_ < SENDING_LOGS)
    return;  // We didn't and still don't have time to get plugin list etc.

  CloseCurrentLog();
  log_store()->PersistUnsentLogs();
}

//------------------------------------------------------------------------------
// Transmission of logs methods

void MetricsService::StartSchedulerIfNecessary() {
  // Never schedule cutting or uploading of logs in test mode.
  if (test_mode_active_)
    return;

  // Even if reporting is disabled, the scheduler is needed to trigger the
  // creation of the initial log, which must be done in order for any logs to be
  // persisted on shutdown or backgrounding.
  if (recording_active() &&
      (reporting_active() || state_ < SENDING_LOGS)) {
    rotation_scheduler_->Start();
    reporting_service_.Start();
  }
}

void MetricsService::StartScheduledUpload() {
  DVLOG(1) << "StartScheduledUpload";
  DCHECK(state_ >= INIT_TASK_DONE);

  // If we're getting no notifications, then the log won't have much in it, and
  // it's possible the computer is about to go to sleep, so don't upload and
  // stop the scheduler.
  // If recording has been turned off, the scheduler doesn't need to run.
  // If reporting is off, proceed if the initial log hasn't been created, since
  // that has to happen in order for logs to be cut and stored when persisting.
  // TODO(stuartmorgan): Call Stop() on the scheduler when reporting and/or
  // recording are turned off instead of letting it fire and then aborting.
  if (idle_since_last_transmission_ ||
      !recording_active() ||
      (!reporting_active() && state_ >= SENDING_LOGS)) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  // If there are unsent logs, send the next one. If not, start the asynchronous
  // process of finalizing the current log for upload.
  if (state_ == SENDING_LOGS && has_unsent_logs()) {
    reporting_service_.Start();
    rotation_scheduler_->RotationFinished();
  } else {
    // There are no logs left to send, so start creating a new one.
    client_->CollectFinalMetricsForLog(
        base::Bind(&MetricsService::OnFinalLogInfoCollectionDone,
                   self_ptr_factory_.GetWeakPtr()));
  }
}

void MetricsService::OnFinalLogInfoCollectionDone() {
  DVLOG(1) << "OnFinalLogInfoCollectionDone";

  // Abort if metrics were turned off during the final info gathering.
  if (!recording_active()) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  if (state_ == INIT_TASK_DONE) {
    PrepareInitialMetricsLog();
  } else {
    DCHECK_EQ(SENDING_LOGS, state_);
    CloseCurrentLog();
    OpenNewLog();
  }
  reporting_service_.Start();
  rotation_scheduler_->RotationFinished();
  HandleIdleSinceLastTransmission(true);
}

bool MetricsService::PrepareInitialStabilityLog(
    const std::string& prefs_previous_version) {
  DCHECK_EQ(INITIALIZED, state_);

  std::unique_ptr<MetricsLog> initial_stability_log(
      CreateLog(MetricsLog::INITIAL_STABILITY_LOG));

  // Do not call OnDidCreateMetricsLog here because the stability
  // log describes stats from the _previous_ session.
  std::string system_profile_app_version;
  if (!initial_stability_log->LoadSavedEnvironmentFromPrefs(
          local_state_, &system_profile_app_version)) {
    return false;
  }
  if (system_profile_app_version != prefs_previous_version)
    StabilityMetricsProvider(local_state_).LogStabilityVersionMismatch();

  log_manager_.PauseCurrentLog();
  log_manager_.BeginLoggingWithLog(std::move(initial_stability_log));

  // Note: Some stability providers may record stability stats via histograms,
  //       so this call has to be after BeginLoggingWithLog().
  log_manager_.current_log()->RecordPreviousSessionData(&delegating_provider_);
  RecordCurrentStabilityHistograms();

  DVLOG(1) << "Generated an stability log.";
  log_manager_.FinishCurrentLog(log_store());
  log_manager_.ResumePausedLog();

  // Store unsent logs, including the stability log that was just saved, so
  // that they're not lost in case of a crash before upload time.
  log_store()->PersistUnsentLogs();

  return true;
}

void MetricsService::PrepareInitialMetricsLog() {
  DCHECK_EQ(INIT_TASK_DONE, state_);

  RecordCurrentEnvironment(initial_metrics_log_.get(), /*complete=*/true);
  base::TimeDelta incremental_uptime;
  base::TimeDelta uptime;
  GetUptimes(local_state_, &incremental_uptime, &uptime);

  // Histograms only get written to the current log, so make the new log current
  // before writing them.
  log_manager_.PauseCurrentLog();
  log_manager_.BeginLoggingWithLog(std::move(initial_metrics_log_));

  // Note: Some stability providers may record stability stats via histograms,
  //       so this call has to be after BeginLoggingWithLog().
  log_manager_.current_log()->RecordCurrentSessionData(
      &delegating_provider_, base::TimeDelta(), base::TimeDelta());
  RecordCurrentHistograms();

  DVLOG(1) << "Generated an initial log.";
  log_manager_.FinishCurrentLog(log_store());
  log_manager_.ResumePausedLog();

  // Store unsent logs, including the initial log that was just saved, so
  // that they're not lost in case of a crash before upload time.
  log_store()->PersistUnsentLogs();

  state_ = SENDING_LOGS;
}

void MetricsService::IncrementLongPrefsValue(const char* path) {
  int64_t value = local_state_->GetInt64(path);
  local_state_->SetInt64(path, value + 1);
}

bool MetricsService::UmaMetricsProperlyShutdown() {
  CHECK(clean_shutdown_status_ == CLEANLY_SHUTDOWN ||
        clean_shutdown_status_ == NEED_TO_SHUTDOWN);
  return clean_shutdown_status_ == CLEANLY_SHUTDOWN;
}

void MetricsService::RegisterMetricsProvider(
    std::unique_ptr<MetricsProvider> provider) {
  DCHECK_EQ(INITIALIZED, state_);
  delegating_provider_.RegisterMetricsProvider(std::move(provider));
}

void MetricsService::CheckForClonedInstall() {
  state_manager_->CheckForClonedInstall();
}

std::unique_ptr<MetricsLog> MetricsService::CreateLog(
    MetricsLog::LogType log_type) {
  return std::make_unique<MetricsLog>(state_manager_->client_id(), session_id_,
                                      log_type, client_);
}

void MetricsService::SetPersistentSystemProfile(
    const std::string& serialized_proto,
    bool complete) {
  GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
      serialized_proto, complete);
}

// static
std::string MetricsService::RecordCurrentEnvironmentHelper(
    MetricsLog* log,
    PrefService* local_state,
    DelegatingProvider* delegating_provider) {
  const SystemProfileProto& system_profile =
      log->RecordEnvironment(delegating_provider);
  EnvironmentRecorder recorder(local_state);
  return recorder.SerializeAndRecordEnvironmentToPrefs(system_profile);
}

void MetricsService::RecordCurrentEnvironment(MetricsLog* log, bool complete) {
  DCHECK(client_);
  std::string serialized_proto =
      RecordCurrentEnvironmentHelper(log, local_state_, &delegating_provider_);

  SetPersistentSystemProfile(serialized_proto, complete);
  client_->OnEnvironmentUpdate(&serialized_proto);
}

void MetricsService::RecordCurrentHistograms() {
  DCHECK(log_manager_.current_log());

  // "true" indicates that StatisticsRecorder should include histograms held in
  // persistent storage.
  base::StatisticsRecorder::PrepareDeltas(
      true, base::Histogram::kNoFlags,
      base::Histogram::kUmaTargetedHistogramFlag, &histogram_snapshot_manager_);
  delegating_provider_.RecordHistogramSnapshots(&histogram_snapshot_manager_);
}

void MetricsService::RecordCurrentStabilityHistograms() {
  DCHECK(log_manager_.current_log());
  // "true" indicates that StatisticsRecorder should include histograms held in
  // persistent storage.
  base::StatisticsRecorder::PrepareDeltas(
      true, base::Histogram::kNoFlags,
      base::Histogram::kUmaStabilityHistogramFlag,
      &histogram_snapshot_manager_);
  delegating_provider_.RecordInitialHistogramSnapshots(
      &histogram_snapshot_manager_);
}

void MetricsService::PrepareProviderMetricsLogDone(
    std::unique_ptr<MetricsLog::IndependentMetricsLoader> loader,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(independent_loader_active_);
  DCHECK(loader);

  if (success) {
    log_manager_.PauseCurrentLog();
    log_manager_.BeginLoggingWithLog(loader->ReleaseLog());
    log_manager_.FinishCurrentLog(log_store());
    log_manager_.ResumePausedLog();
  }

  independent_loader_active_ = false;
}

bool MetricsService::PrepareProviderMetricsLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If something is still pending, stop now and indicate that there is
  // still work to do.
  if (independent_loader_active_)
    return true;

  // Check each provider in turn for data.
  for (auto& provider : delegating_provider_.GetProviders()) {
    if (provider->HasIndependentMetrics()) {
      // Create a new log. This will have some default values injected in it
      // but those will be overwritten when an embedded profile is extracted.
      std::unique_ptr<MetricsLog> log = CreateLog(MetricsLog::INDEPENDENT_LOG);

      // Note that something is happening. This must be set before the
      // operation is requested in case the loader decides to do everything
      // immediately rather than as a background task.
      independent_loader_active_ = true;

      // Give the new log to a loader for management and then run it on the
      // provider that has something to give. A copy of the pointer is needed
      // because the unique_ptr may get moved before the value can be used
      // to call Run().
      std::unique_ptr<MetricsLog::IndependentMetricsLoader> loader =
          std::make_unique<MetricsLog::IndependentMetricsLoader>(
              std::move(log));
      MetricsLog::IndependentMetricsLoader* loader_ptr = loader.get();
      loader_ptr->Run(
          base::BindOnce(&MetricsService::PrepareProviderMetricsLogDone,
                         self_ptr_factory_.GetWeakPtr(), std::move(loader)),
          provider.get());

      // Something was found so there may still be more work to do.
      return true;
    }
  }

  // Nothing was found so indicate there is no more work to do.
  return false;
}

void MetricsService::PrepareProviderMetricsTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool found = PrepareProviderMetricsLog();
  base::TimeDelta next_check = found ? base::TimeDelta::FromSeconds(5)
                                     : base::TimeDelta::FromMinutes(15);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsService::PrepareProviderMetricsTask,
                     self_ptr_factory_.GetWeakPtr()),
      next_check);
}

void MetricsService::LogCleanShutdown(bool end_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Redundant setting to assure that we always reset this value at shutdown
  // (and that we don't use some alternate path, and not call LogCleanShutdown).
  clean_shutdown_status_ = CLEANLY_SHUTDOWN;
  client_->OnLogCleanShutdown();
  state_manager_->clean_exit_beacon()->WriteBeaconValue(true);
  StabilityMetricsProvider(local_state_).MarkSessionEndCompleted(end_completed);
}

void MetricsService::UpdateLastLiveTimestampTask() {
  state_manager_->clean_exit_beacon()->UpdateLastLiveTimestamp();

  // Schecule the next update.
  StartUpdatingLastLiveTimestamp();
}

}  // namespace metrics
