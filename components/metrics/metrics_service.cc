// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Description of a MetricsService instance's life cycle.
//
// OVERVIEW
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
// terminations is done using the PrefServices facilities. The retained logs
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
//  CONSTRUCTED,          // Constructor was called.
//  INITIALIZED,          // InitializeMetricsRecordingState() was called.
//  INIT_TASK_SCHEDULED,  // Waiting for deferred init tasks to finish.
//  INIT_TASK_DONE,       // Waiting for timer to send the first ongoing log.
//  SENDING_LOGS,         // Sending logs and creating new ones when we run out.
//
// In more detail, we have:
//
//    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to finish.
// Typically about 30 seconds after startup, a task is sent to a background
// thread to perform deferred (lower priority and slower) initialization steps
// such as getting the list of plugins.  That task will (when complete) make an
// async callback (via a Task) to indicate the completion.
//
//    INIT_TASK_DONE,         // Waiting for timer to send first ongoing log.
// The callback has arrived, and it is now possible for an ongoing log to be
// created.  This callback typically arrives back less than one second after
// the deferred init task is dispatched.
//
//    SENDING_LOGS,  // Sending logs and creating new ones when we run out.
// Logs from previous sessions have been loaded, and an optional initial
// stability log has been created. We will send all of these logs, and when
// they run out, we will start cutting new logs to send.  We will also cut a new
// log if we expect a shutdown.
//
// The progression through the above states is simple, and sequential.
// States proceed from INITIALIZED to SENDING_LOGS, and remain in the latter
// until shutdown.
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
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_manager.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_service_observer.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/stability_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/metrics/structured/neutrino_logging.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace metrics {
namespace {

// Used to write histogram data to a log. Does not take ownership of the log.
class IndependentFlattener : public base::HistogramFlattener {
 public:
  explicit IndependentFlattener(MetricsLog* log) : log_(log) {}

  IndependentFlattener(const IndependentFlattener&) = delete;
  IndependentFlattener& operator=(const IndependentFlattener&) = delete;

  ~IndependentFlattener() override = default;

  // base::HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    log_->RecordHistogramDelta(histogram.histogram_name(), snapshot);
  }

 private:
  const raw_ptr<MetricsLog, DanglingUntriaged> log_;
};

// Used to mark histogram samples as reported so that they are not included in
// the next log. A histogram's snapshot samples are simply discarded/ignored
// when attempting to record them through this |HistogramFlattener|.
class DiscardingFlattener : public base::HistogramFlattener {
 public:
  DiscardingFlattener() = default;

  DiscardingFlattener(const DiscardingFlattener&) = delete;
  DiscardingFlattener& operator=(const DiscardingFlattener&) = delete;

  ~DiscardingFlattener() override = default;

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    // No-op. We discard the samples.
  }
};

// The delay, in seconds, after starting recording before doing expensive
// initialization work.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
enum UserLogStoreState {
  kSetPostSendLogsState = 0,
  kSetPreSendLogsState = 1,
  kUnsetPostSendLogsState = 2,
  kUnsetPreSendLogsState = 3,
  kMaxValue = kUnsetPreSendLogsState,
};

void RecordUserLogStoreState(UserLogStoreState state) {
  base::UmaHistogramEnumeration("UMA.CrosPerUser.UserLogStoreState", state);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// static
void MetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  CleanExitBeacon::RegisterPrefs(registry);
  MetricsStateManager::RegisterPrefs(registry);
  MetricsLog::RegisterPrefs(registry);
  StabilityMetricsProvider::RegisterPrefs(registry);
  MetricsReportingService::RegisterPrefs(registry);

  registry->RegisterIntegerPref(prefs::kMetricsSessionID, -1);
}

MetricsService::MetricsService(MetricsStateManager* state_manager,
                               MetricsServiceClient* client,
                               PrefService* local_state)
    : reporting_service_(client, local_state, &logs_event_manager_),
      state_manager_(state_manager),
      client_(client),
      local_state_(local_state),
      recording_state_(UNSET),
      test_mode_active_(false),
      state_(CONSTRUCTED),
      idle_since_last_transmission_(false),
      session_id_(-1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_manager_);
  DCHECK(client_);
  DCHECK(local_state_);

  bool create_logs_event_observer;
#ifdef NDEBUG
  // For non-debug builds, we only create |logs_event_observer_| if the
  // |kExportUmaLogsToFile| command line flag is passed. This is mostly for
  // performance reasons: 1) we don't want to have to notify an observer in
  // non-debug circumstances (there may be heavy work like copying large
  // strings), and 2) we don't want logs to be lingering in memory.
  create_logs_event_observer =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExportUmaLogsToFile);
#else
  // For debug builds, always create |logs_event_observer_|.
  create_logs_event_observer = true;
#endif  // NDEBUG

  if (create_logs_event_observer) {
    logs_event_observer_ = std::make_unique<MetricsServiceObserver>(
        MetricsServiceObserver::MetricsServiceType::UMA);
    logs_event_manager_.AddObserver(logs_event_observer_.get());
  }

  RegisterMetricsProvider(
      std::make_unique<StabilityMetricsProvider>(local_state_));

  RegisterMetricsProvider(state_manager_->GetProvider());
}

MetricsService::~MetricsService() {
  DisableRecording();

  if (logs_event_observer_) {
    logs_event_manager_.RemoveObserver(logs_event_observer_.get());
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kExportUmaLogsToFile)) {
      // We should typically not write to files on the main thread, but since
      // this only happens when |kExportUmaLogsToFile| is passed (which
      // indicates debugging), this should be fine.
      logs_event_observer_->ExportLogsToFile(
          command_line->GetSwitchValuePath(switches::kExportUmaLogsToFile));
    }
  }
}

void MetricsService::InitializeMetricsRecordingState() {
  DCHECK_EQ(CONSTRUCTED, state_);

  // The FieldTrialsProvider should be registered last. This ensures that
  // studies whose features are checked when providers add their information to
  // the log appear in the active field trials.
  RegisterMetricsProvider(std::make_unique<variations::FieldTrialsProvider>(
      client_->GetSyntheticTrialRegistry(), base::StringPiece()));

  reporting_service_.Initialize();
  InitializeMetricsState();

  base::RepeatingClosure upload_callback = base::BindRepeating(
      &MetricsService::StartScheduledUpload, self_ptr_factory_.GetWeakPtr());

  rotation_scheduler_ = std::make_unique<MetricsRotationScheduler>(
      upload_callback,
      // MetricsServiceClient outlives MetricsService, and
      // MetricsRotationScheduler is tied to the lifetime of |this|.
      base::BindRepeating(&MetricsServiceClient::GetUploadInterval,
                          base::Unretained(client_)),
      client_->ShouldStartUpFastForTesting());

  // Init() has to be called after LogCrash() in order for LogCrash() to work.
  delegating_provider_.Init();

  state_ = INITIALIZED;
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsService::UpdateLastLiveTimestampTask,
                     self_ptr_factory_.GetWeakPtr()),
      base::Seconds(kUpdateAliveTimestampSeconds));
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

std::string MetricsService::GetClientId() const {
  return state_manager_->client_id();
}

void MetricsService::SetExternalClientId(const std::string& id) {
  state_manager_->SetExternalClientId(id);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This must be after OnRecordingEnabled() to ensure that the structured
  // logging has been enabled.
  metrics::structured::NeutrinoDevicesLogWithClientId(
      state_manager_->client_id(),
      metrics::structured::NeutrinoDevicesLocation::kEnableRecording);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Fill in the system profile in the log and persist it (to prefs, .pma
  // and crashpad). This includes running the providers so that information
  // like field trials and hardware info is provided. If Chrome crashes
  // before this log is completed, the .pma file will have this system
  // profile.
  RecordCurrentEnvironment(log_manager_.current_log(), /*complete=*/false);

  base::RemoveActionCallback(action_callback_);
  action_callback_ = base::BindRepeating(&MetricsService::OnUserAction,
                                         base::Unretained(this));
  base::AddActionCallback(action_callback_);

  enablement_observers_.Notify(/*enabled=*/true);
}

void MetricsService::DisableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (recording_state_ == INACTIVE)
    return;
  recording_state_ = INACTIVE;

  base::RemoveActionCallback(action_callback_);

  delegating_provider_.OnRecordingDisabled();

  base::UmaHistogramBoolean("UMA.MetricsService.PendingOngoingLogOnDisable",
                            pending_ongoing_log_);
  PushPendingLogsToPersistentStorage();

  // If kEmitHistogramsForIndependentLogs is set, call OnDidCreateMetricsLog()
  // to provide histograms.
  if (base::FeatureList::IsEnabled(features::kEmitHistogramsEarlier) &&
      features::kEmitHistogramsForIndependentLogs.Get()) {
    delegating_provider_.OnDidCreateMetricsLog();
  }

  enablement_observers_.Notify(/*enabled=*/false);
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

bool MetricsService::IsMetricsReportingEnabled() const {
  return state_manager_->IsMetricsReportingEnabled();
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void MetricsService::OnAppEnterBackground(bool keep_recording_in_background) {
  is_in_foreground_ = false;
  if (!keep_recording_in_background) {
    rotation_scheduler_->Stop();
    reporting_service_.Stop();
  }

  state_manager_->LogHasSessionShutdownCleanly(true);
  // Schedule a write, which happens on a different thread.
  local_state_->CommitPendingWrite();

  // Give providers a chance to persist histograms as part of being
  // backgrounded.
  delegating_provider_.OnAppEnterBackground();

  // At this point, there's no way of knowing when the process will be
  // killed, so this has to be treated similar to a shutdown, closing and
  // persisting all logs. Unlinke a shutdown, the state is primed to be ready
  // to continue logging and uploading if the process does return.
  if (recording_active() && state_ >= SENDING_LOGS) {
    base::UmaHistogramBoolean(
        "UMA.MetricsService.PendingOngoingLogOnBackgrounded",
        pending_ongoing_log_);
    PushPendingLogsToPersistentStorage();
    // Persisting logs closes the current log, so start recording a new log
    // immediately to capture any background work that might be done before the
    // process is killed.
    OpenNewLog();
  }
}

void MetricsService::OnAppEnterForeground(bool force_open_new_log) {
  is_in_foreground_ = true;
  state_manager_->LogHasSessionShutdownCleanly(false);
  StartSchedulerIfNecessary();

  if (force_open_new_log && recording_active() && state_ >= SENDING_LOGS) {
    base::UmaHistogramBoolean(
        "UMA.MetricsService.PendingOngoingLogOnForegrounded",
        pending_ongoing_log_);
    // Because state_ >= SENDING_LOGS, PushPendingLogsToPersistentStorage()
    // will close the log, allowing a new log to be opened.
    PushPendingLogsToPersistentStorage();
    OpenNewLog();
  }
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

void MetricsService::LogCleanShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_manager_->LogHasSessionShutdownCleanly(true);
}

void MetricsService::ClearSavedStabilityMetrics() {
  delegating_provider_.ClearSavedStabilityMetrics();
  // Stability metrics are stored in Local State prefs, so schedule a Local
  // State write to flush the updated prefs.
  local_state_->CommitPendingWrite();
}

void MetricsService::MarkCurrentHistogramsAsReported() {
  DiscardingFlattener flattener;
  base::HistogramSnapshotManager snapshot_manager(&flattener);
  base::StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/true, /*flags_to_set=*/base::Histogram::kNoFlags,
      /*required_flags=*/base::Histogram::kUmaTargetedHistogramFlag,
      &snapshot_manager);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MetricsService::SetUserLogStore(
    std::unique_ptr<UnsentLogStore> user_log_store) {
  if (log_store()->has_alternate_ongoing_log_store())
    return;

  if (state_ >= SENDING_LOGS) {
    // Closes the current log so that a new log can be opened in the user log
    // store.
    PushPendingLogsToPersistentStorage();
    log_store()->SetAlternateOngoingLogStore(std::move(user_log_store));
    OpenNewLog();
    RecordUserLogStoreState(kSetPostSendLogsState);
  } else {
    // Initial log has not yet been created and flushing now would result in
    // incomplete information in the current log.
    //
    // Logs recorded before a user login will be appended to user logs. This
    // should not happen frequently.
    //
    // TODO(crbug/1264627): Look for a way to "pause" pre-login logs and flush
    // when INIT_TASK is done.
    log_store()->SetAlternateOngoingLogStore(std::move(user_log_store));
    RecordUserLogStoreState(kSetPreSendLogsState);
  }
}

void MetricsService::UnsetUserLogStore() {
  if (!log_store()->has_alternate_ongoing_log_store())
    return;

  if (state_ >= SENDING_LOGS) {
    PushPendingLogsToPersistentStorage();
    log_store()->UnsetAlternateOngoingLogStore();
    OpenNewLog();
    RecordUserLogStoreState(kUnsetPostSendLogsState);
    return;
  }

  // Fast startup and logout case. We flush all histograms and discard the
  // current log. This is to prevent histograms captured during the user
  // session from leaking into local state logs.
  // TODO(crbug/1381581): Consider not flushing histograms here.

  // Discard histograms.
  DiscardingFlattener flattener;
  base::HistogramSnapshotManager histogram_snapshot_manager(&flattener);
  delegating_provider_.RecordHistogramSnapshots(&histogram_snapshot_manager);
  base::StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/true, /*flags_to_set=*/base::Histogram::kNoFlags,
      /*required_flags=*/base::Histogram::kUmaTargetedHistogramFlag,
      &histogram_snapshot_manager);

  // Release the current log and don't store it (i.e., we discard it).
  log_manager_.ReleaseCurrentLog();

  log_store()->UnsetAlternateOngoingLogStore();
  RecordUserLogStoreState(kUnsetPreSendLogsState);
}

bool MetricsService::HasUserLogStore() {
  return log_store()->has_alternate_ongoing_log_store();
}

void MetricsService::InitPerUserMetrics() {
  client_->InitPerUserMetrics();
}

absl::optional<bool> MetricsService::GetCurrentUserMetricsConsent() const {
  return client_->GetCurrentUserMetricsConsent();
}

absl::optional<std::string> MetricsService::GetCurrentUserId() const {
  return client_->GetCurrentUserId();
}

void MetricsService::UpdateCurrentUserMetricsConsent(
    bool user_metrics_consent) {
  client_->UpdateCurrentUserMetricsConsent(user_metrics_consent);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
void MetricsService::ResetClientId() {
  // Pref must be cleared in order for ForceClientIdCreation to generate a new
  // client ID.
  local_state_->ClearPref(prefs::kMetricsClientID);
  state_manager_->ForceClientIdCreation();
  client_->SetMetricsClientId(state_manager_->client_id());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

variations::SyntheticTrialRegistry*
MetricsService::GetSyntheticTrialRegistry() {
  return client_->GetSyntheticTrialRegistry();
}

bool MetricsService::StageCurrentLogForTest() {
  CloseCurrentLog(/*async=*/false);

  MetricsLogStore* const log_store = reporting_service_.metrics_log_store();
  log_store->StageNextLog();
  if (!log_store->has_staged_log())
    return false;

  OpenNewLog();
  return true;
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Initialization methods

void MetricsService::InitializeMetricsState() {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("UMA.MetricsService.Initialize.Time");

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
  const bool was_last_shutdown_clean = WasLastShutdownClean();
  if (!was_last_shutdown_clean) {
    provider.LogCrash(
        state_manager_->clean_exit_beacon()->browser_last_live_timestamp());
#if BUILDFLAG(IS_ANDROID)
    if (!state_manager_->is_foreground_session()) {
      // Android can have background sessions in which the app may not come to
      // the foreground, so signal that Chrome should stop watching for crashes
      // here. This ensures that the termination of such sessions is not
      // considered a crash. If and when the app enters the foreground, Chrome
      // starts watching for crashes via MetricsService::OnAppEnterForeground().
      //
      // TODO(crbug/1232027): Such sessions do not yet exist on iOS. When they
      // do, it may not be possible to know at this point whether a session is a
      // background session.
      //
      // TODO(crbug/1245347): On WebLayer, it is not possible to know whether
      // it's a background session at this point.
      //
      // TODO(crbug/1245676): Ditto for WebView.
      state_manager_->clean_exit_beacon()->WriteBeaconValue(true);
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // HasPreviousSessionData is called first to ensure it is never bypassed.
  const bool is_initial_stability_log_required =
      delegating_provider_.HasPreviousSessionData() || !was_last_shutdown_clean;
  bool has_initial_stability_log = false;
  if (is_initial_stability_log_required) {
    // If the previous session didn't exit cleanly, or if any provider
    // explicitly requests it, prepare an initial stability log -
    // provided UMA is enabled.
    if (state_manager_->IsMetricsReportingEnabled()) {
      has_initial_stability_log = PrepareInitialStabilityLog(previous_version);
    }
  }

  // If the version changed, but no initial stability log was generated, clear
  // the stability stats from the previous version (so that they don't get
  // attributed to the current version). This could otherwise happen due to a
  // number of different edge cases, such as if the last version crashed before
  // it could save off a system profile or if UMA reporting is disabled (which
  // normally results in stats being accumulated).
  if (version_changed && !has_initial_stability_log)
    ClearSavedStabilityMetrics();

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

  // Call GetUptimes() for the first time, thus allowing all later calls
  // to record incremental uptimes accurately.
  base::TimeDelta ignored_uptime_parameter;
  base::TimeDelta startup_uptime;
  GetUptimes(local_state_, &startup_uptime, &ignored_uptime_parameter);
  DCHECK_EQ(0, startup_uptime.InMicroseconds());
}

void MetricsService::OnUserAction(const std::string& action,
                                  base::TimeTicks action_time) {
  log_manager_.current_log()->RecordUserAction(action, action_time);
  HandleIdleSinceLastTransmission(false);
}

void MetricsService::FinishedInitTask() {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
  state_ = INIT_TASK_DONE;
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
}

//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::OpenNewLog(bool call_providers) {
  DCHECK(!log_manager_.current_log());

  log_manager_.BeginLoggingWithLog(CreateLog(MetricsLog::ONGOING_LOG));
  if (call_providers) {
    delegating_provider_.OnDidCreateMetricsLog();
  }

  DCHECK_NE(CONSTRUCTED, state_);
  if (state_ == INITIALIZED) {
    // We only need to schedule that run once.
    state_ = INIT_TASK_SCHEDULED;

    base::TimeDelta initialization_delay = base::Seconds(
        client_->ShouldStartUpFastForTesting() ? 0
                                               : kInitializationDelaySeconds);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MetricsService::StartInitTask,
                       self_ptr_factory_.GetWeakPtr()),
        initialization_delay);

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MetricsService::PrepareProviderMetricsTask,
                       self_ptr_factory_.GetWeakPtr()),
        2 * initialization_delay);
  }
}

MetricsService::FinalizedLog::FinalizedLog() = default;
MetricsService::FinalizedLog::~FinalizedLog() = default;
MetricsService::FinalizedLog::FinalizedLog(FinalizedLog&& other) = default;

MetricsService::MetricsLogHistogramWriter::MetricsLogHistogramWriter(
    MetricsLog* log)
    : MetricsLogHistogramWriter(log,
                                base::Histogram::kUmaTargetedHistogramFlag) {}

MetricsService::MetricsLogHistogramWriter::MetricsLogHistogramWriter(
    MetricsLog* log,
    base::HistogramBase::Flags required_flags)
    : required_flags_(required_flags),
      flattener_(std::make_unique<IndependentFlattener>(log)),
      histogram_snapshot_manager_(
          std::make_unique<base::HistogramSnapshotManager>(flattener_.get())),
      snapshot_transaction_id_(0) {}

MetricsService::MetricsLogHistogramWriter::~MetricsLogHistogramWriter() =
    default;

void MetricsService::MetricsLogHistogramWriter::
    SnapshotStatisticsRecorderDeltas() {
  snapshot_transaction_id_ = base::StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/true,
      /*flags_to_set=*/base::Histogram::kNoFlags, required_flags_,
      histogram_snapshot_manager_.get());
}

void MetricsService::MetricsLogHistogramWriter::
    SnapshotStatisticsRecorderUnloggedSamples() {
  snapshot_transaction_id_ = base::StatisticsRecorder::SnapshotUnloggedSamples(
      required_flags_, histogram_snapshot_manager_.get());
}

MetricsService::IndependentMetricsLoader::IndependentMetricsLoader(
    std::unique_ptr<MetricsLog> log)
    : log_(std::move(log)),
      flattener_(new IndependentFlattener(log_.get())),
      snapshot_manager_(new base::HistogramSnapshotManager(flattener_.get())) {}

MetricsService::IndependentMetricsLoader::~IndependentMetricsLoader() = default;

void MetricsService::IndependentMetricsLoader::Run(
    base::OnceCallback<void(bool)> done_callback,
    MetricsProvider* metrics_provider) {
  metrics_provider->ProvideIndependentMetrics(
      std::move(done_callback), log_->uma_proto(), snapshot_manager_.get());
}

std::unique_ptr<MetricsLog>
MetricsService::IndependentMetricsLoader::ReleaseLog() {
  return std::move(log_);
}

void MetricsService::StartInitTask() {
  delegating_provider_.AsyncInit(base::BindOnce(
      &MetricsService::FinishedInitTask, self_ptr_factory_.GetWeakPtr()));
}

void MetricsService::CloseCurrentLog(bool async,
                                     base::OnceClosure log_stored_callback) {
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
  std::unique_ptr<MetricsLog> current_log = log_manager_.ReleaseCurrentLog();
  DCHECK(current_log);
  RecordCurrentEnvironment(current_log.get(), /*complete=*/true);
  base::TimeDelta incremental_uptime;
  base::TimeDelta uptime;
  GetUptimes(local_state_, &incremental_uptime, &uptime);
  current_log->RecordCurrentSessionData(incremental_uptime, uptime,
                                        &delegating_provider_, local_state_);

  auto log_histogram_writer =
      std::make_unique<MetricsLogHistogramWriter>(current_log.get());

  // Let metrics providers provide histogram snapshots independently if they
  // have any. This is done synchronously.
  delegating_provider_.RecordHistogramSnapshots(
      log_histogram_writer->histogram_snapshot_manager());

  MetricsLog::LogType log_type = current_log->log_type();
  std::string signing_key = log_store()->GetSigningKeyForLogType(log_type);
  if (async) {
    // To finalize the log asynchronously, we snapshot the unlogged samples of
    // histograms and fill them into the log, without actually marking the
    // samples as logged. We only mark them as logged after running the main
    // thread reply task to store the log. This way, we will not lose the
    // samples in case Chrome closes while the background task is running. Note
    // that while this async log is being finalized, it is possible that another
    // log is finalized and stored synchronously, which could potentially cause
    // the same samples to be in two different logs, and hence sent twice. To
    // prevent this, if a synchronous log is stored while the async one is being
    // finalized, we discard the async log as it would be a subset of the
    // synchronous one (in terms of histograms). For more details, see
    // MaybeCleanUpAndStoreFinalizedLog().
    //
    // TODO(crbug/1052796): Find a way to save the other data such as user
    // actions and omnibox events when we discard an async log.
    MetricsLogHistogramWriter* log_histogram_writer_ptr =
        log_histogram_writer.get();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MetricsService::SnapshotUnloggedSamplesAndFinalizeLog,
                       log_histogram_writer_ptr, std::move(current_log),
                       /*truncate_events=*/true, client_->GetVersionString(),
                       std::move(signing_key)),
        base::BindOnce(&MetricsService::MaybeCleanUpAndStoreFinalizedLog,
                       self_ptr_factory_.GetWeakPtr(),
                       std::move(log_histogram_writer), log_type,
                       std::move(log_stored_callback)));
    async_ongoing_log_posted_time_ = base::TimeTicks::Now();
  } else {
    FinalizedLog finalized_log = SnapshotDeltasAndFinalizeLog(
        std::move(log_histogram_writer), std::move(current_log),
        /*truncate_events=*/true, client_->GetVersionString(),
        std::move(signing_key));
    StoreFinalizedLog(log_type, std::move(log_stored_callback),
                      std::move(finalized_log));
  }
}

void MetricsService::StoreFinalizedLog(MetricsLog::LogType log_type,
                                       base::OnceClosure done_callback,
                                       FinalizedLog finalized_log) {
  log_store()->StoreLogInfo(std::move(finalized_log.log_info),
                            finalized_log.uncompressed_log_size, log_type);
  std::move(done_callback).Run();
}

void MetricsService::MaybeCleanUpAndStoreFinalizedLog(
    std::unique_ptr<MetricsLogHistogramWriter> log_histogram_writer,
    MetricsLog::LogType log_type,
    base::OnceClosure done_callback,
    FinalizedLog finalized_log) {
  UMA_HISTOGRAM_TIMES("UMA.MetricsService.PeriodicOngoingLog.ReplyTime",
                      base::TimeTicks::Now() - async_ongoing_log_posted_time_);

  // Store the finalized log only if the StatisticRecorder's last transaction ID
  // is the same as the one from |log_histogram_writer|. If they are not the
  // same, then it indicates that another log was created while creating
  // |finalized_log| (that log would be a superset of |finalized_log| in terms
  // of histograms, so we discard |finalized_log| by not storing it).
  //
  // TODO(crbug/1052796): Find a way to save the other data such as user actions
  // and omnibox events when we discard |finalized_log|.
  //
  // Note that the call to StatisticsRecorder::GetLastSnapshotTransactionId()
  // here should not have to wait for a lock since there should not be any async
  // logs being created (|rotation_scheduler_| is only re-scheduled at the end
  // of this method).
  bool should_store_log =
      (base::StatisticsRecorder::GetLastSnapshotTransactionId() ==
       log_histogram_writer->snapshot_transaction_id());
  base::UmaHistogramBoolean("UMA.MetricsService.ShouldStoreAsyncLog",
                            should_store_log);

  if (!should_store_log) {
    // We still need to run |done_callback| even if we do not store the log.
    std::move(done_callback).Run();
    return;
  }

  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.MetricsService.MaybeCleanUpAndStoreFinalizedLog.Time");

  log_histogram_writer->histogram_snapshot_manager()
      ->MarkUnloggedSamplesAsLogged();
  StoreFinalizedLog(log_type, std::move(done_callback),
                    std::move(finalized_log));

  // Call OnDidCreateMetricsLog() after storing a log instead of directly after
  // opening a log. Otherwise, the async log that was created would potentially
  // have mistakenly snapshotted the histograms intended for the newly opened
  // log.
  delegating_provider_.OnDidCreateMetricsLog();
}

void MetricsService::PushPendingLogsToPersistentStorage() {
  if (state_ < SENDING_LOGS)
    return;  // We didn't and still don't have time to get plugin list etc.

  base::UmaHistogramBoolean("UMA.MetricsService.PendingOngoingLog",
                            pending_ongoing_log_);

  // Close and store a log synchronously because this is usually called in
  // critical code paths (e.g., shutdown) where we may not have time to run
  // background tasks.
  CloseCurrentLog(/*async=*/false);
  log_store()->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
}

//------------------------------------------------------------------------------
// Transmission of logs methods

void MetricsService::StartSchedulerIfNecessary() {
  // Never schedule cutting or uploading of logs in test mode.
  if (test_mode_active_)
    return;

  // Even if reporting is disabled, the scheduler is needed to trigger the
  // creation of the first ongoing log, which must be done in order for any logs
  // to be persisted on shutdown or backgrounding.
  if (recording_active() && (reporting_active() || state_ < SENDING_LOGS)) {
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
  // If reporting is off, proceed if the first ongoing log hasn't been created,
  // since that has to happen in order for logs to be cut and stored when
  // persisting.
  // TODO(stuartmorgan): Call Stop() on the scheduler when reporting and/or
  // recording are turned off instead of letting it fire and then aborting.
  if (idle_since_last_transmission_ || !recording_active() ||
      (!reporting_active() && state_ >= SENDING_LOGS)) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  // The first ongoing log should be collected prior to sending any unsent logs.
  if (state_ == INIT_TASK_DONE) {
    client_->CollectFinalMetricsForLog(
        base::BindOnce(&MetricsService::OnFinalLogInfoCollectionDone,
                       self_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If there are unsent logs, send the next one. If not, start the asynchronous
  // process of finalizing the current log for upload.
  if (has_unsent_logs()) {
    reporting_service_.Start();
    rotation_scheduler_->RotationFinished();
  } else {
    // There are no logs left to send, so start creating a new one.
    client_->CollectFinalMetricsForLog(
        base::BindOnce(&MetricsService::OnFinalLogInfoCollectionDone,
                       self_ptr_factory_.GetWeakPtr()));
  }
}

void MetricsService::OnFinalLogInfoCollectionDone() {
  DVLOG(1) << "OnFinalLogInfoCollectionDone";
  DCHECK(state_ >= INIT_TASK_DONE);
  state_ = SENDING_LOGS;

  // Abort if metrics were turned off during the final info gathering.
  if (!recording_active()) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("UMA.MetricsService.PeriodicOngoingLog.CloseTime");

  // There shouldn't be two periodic ongoing logs being finalized in the
  // background simultaneously. This is currently enforced because:
  // 1. Only periodic ongoing logs are finalized asynchronously (i.e., logs
  //    created by the MetricsRotationScheduler).
  // 2. We only re-schedule the MetricsRotationScheduler after storing a
  //    periodic ongoing log.
  //
  // TODO(crbug/1052796): Consider making it possible to have multiple
  // simultaneous async logs by having some queueing system (e.g., if we want
  // the log created when foregrounding Chrome to be async).
  DCHECK(!pending_ongoing_log_);
  pending_ongoing_log_ = true;

  base::OnceClosure log_stored_callback =
      base::BindOnce(&MetricsService::OnPeriodicOngoingLogStored,
                     self_ptr_factory_.GetWeakPtr());
  if (base::FeatureList::IsEnabled(features::kMetricsServiceAsyncCollection)) {
    CloseCurrentLog(/*async=*/true, std::move(log_stored_callback));
    OpenNewLog(/*call_providers=*/false);
  } else {
    CloseCurrentLog(/*async=*/false, std::move(log_stored_callback));
    OpenNewLog();
  }
}

void MetricsService::OnPeriodicOngoingLogStored() {
  pending_ongoing_log_ = false;

  // Trim and store unsent logs, including the log that was just closed, so that
  // they're not lost in case of a crash before upload time. However, the
  // in-memory log store is unchanged. I.e., logs that are trimmed will still be
  // available in memory. This is to give the log that was just created a chance
  // to be sent in case it is trimmed. After uploading (whether successful or
  // not), the log store is trimmed and stored again, and at that time, the
  // in-memory log store will be updated.
  log_store()->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/false);

  // Do not re-schedule if metrics were turned off while finalizing the log.
  if (!recording_active()) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
  } else {
    // Only re-schedule |rotation_scheduler_| *after* the log was stored to
    // ensure that only one log is created asynchronously at a time.
    reporting_service_.Start();
    rotation_scheduler_->RotationFinished();
    HandleIdleSinceLastTransmission(true);
  }
}

bool MetricsService::PrepareInitialStabilityLog(
    const std::string& prefs_previous_version) {
  DCHECK_EQ(CONSTRUCTED, state_);

  std::unique_ptr<MetricsLog> initial_stability_log(
      CreateLog(MetricsLog::INITIAL_STABILITY_LOG));

  // Do not call OnDidCreateMetricsLog here because the stability log describes
  // stats from the _previous_ session.

  if (!initial_stability_log->LoadSavedEnvironmentFromPrefs(local_state_))
    return false;

  initial_stability_log->RecordPreviousSessionData(&delegating_provider_,
                                                   local_state_);

  auto log_histogram_writer = std::make_unique<MetricsLogHistogramWriter>(
      initial_stability_log.get(), base::Histogram::kUmaStabilityHistogramFlag);

  // Let metrics providers provide histogram snapshots independently if they
  // have any. This is done synchronously.
  delegating_provider_.RecordInitialHistogramSnapshots(
      log_histogram_writer->histogram_snapshot_manager());

  MetricsLog::LogType log_type = initial_stability_log->log_type();
  std::string signing_key = log_store()->GetSigningKeyForLogType(log_type);

  // Synchronously create the initial stability log in order to ensure that the
  // stability histograms are filled into this specific log.
  FinalizedLog finalized_log = SnapshotDeltasAndFinalizeLog(
      std::move(log_histogram_writer), std::move(initial_stability_log),
      /*truncate_events=*/false, client_->GetVersionString(),
      std::move(signing_key));
  StoreFinalizedLog(log_type, base::DoNothing(), std::move(finalized_log));

  // Store unsent logs, including the stability log that was just saved, so
  // that they're not lost in case of a crash before upload time.
  log_store()->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

  return true;
}

void MetricsService::RegisterMetricsProvider(
    std::unique_ptr<MetricsProvider> provider) {
  DCHECK_EQ(CONSTRUCTED, state_);
  delegating_provider_.RegisterMetricsProvider(std::move(provider));
}

void MetricsService::CheckForClonedInstall() {
  state_manager_->CheckForClonedInstall();
}

bool MetricsService::ShouldResetClientIdsOnClonedInstall() {
  return state_manager_->ShouldResetClientIdsOnClonedInstall();
}

std::unique_ptr<MetricsLog> MetricsService::CreateLog(
    MetricsLog::LogType log_type) {
  auto new_metrics_log = std::make_unique<MetricsLog>(
      state_manager_->client_id(), session_id_, log_type, client_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<std::string> user_id = GetCurrentUserId();
  if (user_id.has_value())
    new_metrics_log->SetUserId(user_id.value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return new_metrics_log;
}

void MetricsService::AddLogsObserver(
    MetricsLogsEventManager::Observer* observer) {
  logs_event_manager_.AddObserver(observer);
}

void MetricsService::RemoveLogsObserver(
    MetricsLogsEventManager::Observer* observer) {
  logs_event_manager_.RemoveObserver(observer);
}

base::CallbackListSubscription MetricsService::AddEnablementObserver(
    const base::RepeatingCallback<void(bool)>& observer) {
  return enablement_observers_.Add(observer);
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

void MetricsService::PrepareProviderMetricsLogDone(
    std::unique_ptr<IndependentMetricsLoader> loader,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(independent_loader_active_);
  DCHECK(loader);

  if (success) {
    // Finalize and store the log that was created independently by the metrics
    // provider.
    std::unique_ptr<MetricsLog> log = loader->ReleaseLog();
    MetricsLog::LogType log_type = log->log_type();
    std::string signing_key = log_store()->GetSigningKeyForLogType(log_type);
    FinalizedLog finalized_log =
        FinalizeLog(std::move(log), /*truncate_events=*/false,
                    client_->GetVersionString(), std::move(signing_key));
    StoreFinalizedLog(log_type, base::DoNothing(), std::move(finalized_log));
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
      std::unique_ptr<IndependentMetricsLoader> loader =
          std::make_unique<IndependentMetricsLoader>(std::move(log));
      IndependentMetricsLoader* loader_ptr = loader.get();
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
  base::TimeDelta next_check = found ? base::Seconds(5) : base::Minutes(15);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsService::PrepareProviderMetricsTask,
                     self_ptr_factory_.GetWeakPtr()),
      next_check);
}

void MetricsService::UpdateLastLiveTimestampTask() {
  state_manager_->clean_exit_beacon()->UpdateLastLiveTimestamp();

  // Schecule the next update.
  StartUpdatingLastLiveTimestamp();
}

// static
MetricsService::FinalizedLog MetricsService::SnapshotDeltasAndFinalizeLog(
    std::unique_ptr<MetricsLogHistogramWriter> log_histogram_writer,
    std::unique_ptr<MetricsLog> log,
    bool truncate_events,
    std::string current_app_version,
    std::string signing_key) {
  log_histogram_writer->SnapshotStatisticsRecorderDeltas();
  return FinalizeLog(std::move(log), truncate_events,
                     std::move(current_app_version), std::move(signing_key));
}

// static
MetricsService::FinalizedLog
MetricsService::SnapshotUnloggedSamplesAndFinalizeLog(
    MetricsLogHistogramWriter* log_histogram_writer,
    std::unique_ptr<MetricsLog> log,
    bool truncate_events,
    std::string current_app_version,
    std::string signing_key) {
  log_histogram_writer->SnapshotStatisticsRecorderUnloggedSamples();
  return FinalizeLog(std::move(log), truncate_events,
                     std::move(current_app_version), std::move(signing_key));
}

// static
MetricsService::FinalizedLog MetricsService::FinalizeLog(
    std::unique_ptr<MetricsLog> log,
    bool truncate_events,
    std::string current_app_version,
    std::string signing_key) {
  std::string log_data;
  log->FinalizeLog(truncate_events, current_app_version, &log_data);

  FinalizedLog finalized_log;
  finalized_log.uncompressed_log_size = log_data.size();
  finalized_log.log_info = std::make_unique<UnsentLogStore::LogInfo>();
  finalized_log.log_info->Init(log_data, signing_key, log->log_metadata());
  return finalized_log;
}

}  // namespace metrics
