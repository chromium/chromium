// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_reporting_service.h"

class PrefService;
class PrefRegistrySimple;
FORWARD_DECLARE_TEST(ChromeMetricsServiceClientTest,
                     TestRegisterMetricsServiceProviders);
FORWARD_DECLARE_TEST(IOSChromeMetricsServiceClientTest,
                     TestRegisterMetricsServiceProviders);

namespace variations {
class SyntheticTrialRegistry;
}

namespace metrics {

class MetricsRotationScheduler;
class MetricsServiceClient;
class MetricsServiceObserver;
class MetricsStateManager;

// See metrics_service.cc for a detailed description.
class MetricsService {
 public:
  // Creates the MetricsService with the given |state_manager|, |client|, and
  // |local_state|.  Does not take ownership of the paramaters; instead stores
  // a weak pointer to each. Caller should ensure that the parameters are valid
  // for the lifetime of this class.
  MetricsService(MetricsStateManager* state_manager,
                 MetricsServiceClient* client,
                 PrefService* local_state);

  MetricsService(const MetricsService&) = delete;
  MetricsService& operator=(const MetricsService&) = delete;

  virtual ~MetricsService();

  // Initializes metrics recording state. Updates various bookkeeping values in
  // prefs and sets up the scheduler. This is a separate function rather than
  // being done by the constructor so that field trials could be created before
  // this is run.
  void InitializeMetricsRecordingState();

  // Starts the metrics system, turning on recording and uploading of metrics.
  // Should be called when starting up with metrics enabled, or when metrics
  // are turned on.
  void Start();

  // Starts the metrics system in a special test-only mode. Metrics won't ever
  // be uploaded or persisted in this mode, but metrics will be recorded in
  // memory.
  void StartRecordingForTests();

  // Starts updating the "last live" browser timestamp.
  void StartUpdatingLastLiveTimestamp();

  // Shuts down the metrics system. Should be called at shutdown, or if metrics
  // are turned off.
  void Stop();

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  // Calling Start() automatically enables reporting, but sending is
  // asyncronous so this can be called immediately after Start() to prevent
  // any uploading.
  void EnableReporting();
  void DisableReporting();

  // Returns the client ID for this client, or the empty string if metrics
  // recording is not currently running.
  std::string GetClientId() const;

  // Get the low entropy source values.
  int GetLowEntropySource();
  int GetOldLowEntropySource();
  int GetPseudoLowEntropySource();

  // Set an external provided id for the metrics service. This method can be
  // set by a caller which wants to explicitly control the *next* id used by the
  // metrics service. Note that setting the external client id will *not* change
  // the current metrics client id. In order to change the current client id,
  // callers should call ResetClientId to change the current client id to the
  // provided id.
  void SetExternalClientId(const std::string& id);

  // Returns the date at which the current metrics client ID was created as
  // an int64_t containing seconds since the epoch.
  int64_t GetMetricsReportingEnabledDate();

  // Returns true if the last session exited cleanly.
  bool WasLastShutdownClean() const;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // This should be called when the application is not idle, i.e. the user seems
  // to be interacting with the application.
  void OnApplicationNotIdle();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Called when the application is going into background mode.
  // If |keep_recording_in_background| is true, UMA is still recorded and
  // reported while in the background.
  void OnAppEnterBackground(bool keep_recording_in_background = false);

  // Called when the application is coming out of background mode.
  void OnAppEnterForeground(bool force_open_new_log = false);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Called when a document first starts loading.
  void OnPageLoadStarted();

  // Signals that the browser is shutting down cleanly. Intended to be called
  // during shutdown after critical shutdown tasks have completed.
  void LogCleanShutdown();

  bool recording_active() const;
  bool reporting_active() const;
  bool has_unsent_logs() const;

  bool IsMetricsReportingEnabled() const;

  // Register the specified |provider| to provide additional metrics into the
  // UMA log. Should be called during MetricsService initialization only.
  void RegisterMetricsProvider(std::unique_ptr<MetricsProvider> provider);

  // Check if this install was cloned or imaged from another machine. If a
  // clone is detected, reset the client id and low entropy source. This
  // should not be called more than once.
  void CheckForClonedInstall();

  // Checks if the cloned install detector says that client ids should be reset.
  bool ShouldResetClientIdsOnClonedInstall();

  // Clears the stability metrics that are saved in local state.
  void ClearSavedStabilityMetrics();

  // Marks current histograms as reported by snapshotting them, without
  // actually saving the deltas. At a higher level, this is used to throw
  // away new histogram samples (since the last log) so that they will not
  // be included in the next log.
  void MarkCurrentHistogramsAsReported();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Binds a user log store to store unsent logs. This log store will be
  // fully managed by MetricsLogStore. This will no-op if another log store has
  // already been set.
  //
  // If this is called before initial logs are recorded, then histograms
  // recorded before user log store is set will be included with user histograms
  // when initial logs are recorded.
  //
  // If this is called after initial logs are recorded, then this will flush all
  // logs recorded before swapping to |user_log_store|.
  void SetUserLogStore(std::unique_ptr<UnsentLogStore> user_log_store);

  // Unbinds the user log store. If there was no user log store, then this does
  // nothing.
  //
  // If this is called before initial logs are recorded, then histograms and the
  // current log will be discarded.
  //
  // If called after initial logs are recorded, then this will flush all logs
  // before the user log store is unset.
  void UnsetUserLogStore();

  // Returns true if a user log store has been bound.
  bool HasUserLogStore();

  // Initializes per-user metrics collection. Logs recorded during a user
  // session will be stored within each user's directory and consent to send
  // these logs will be controlled by each user. Logs recorded before any user
  // logs in or during guest sessions (given device owner has consented) will be
  // stored in local_state.
  //
  // This is in its own function because the MetricsService is created very
  // early on and a user metrics service may have dependencies on services that
  // are created happen after MetricsService is initialized.
  void InitPerUserMetrics();

  // Returns the current user metrics consent if it should be applied to
  // determine metrics reporting state.
  //
  // See comments at MetricsServiceClient::GetCurrentUserMetricsConsent() for
  // more details.
  std::optional<bool> GetCurrentUserMetricsConsent() const;

  // Returns the current logged in user id. See comments at
  // MetricsServiceClient::GetCurrentUserId() for more details.
  std::optional<std::string> GetCurrentUserId() const;

  // Updates the current user metrics consent. No-ops if no user has logged in.
  void UpdateCurrentUserMetricsConsent(bool user_metrics_consent);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  // Forces the client ID to be reset and generates a new client ID. This will
  // be called when a user re-consents to metrics collection and the user had
  // consented in the past.
  //
  // This is to preserve the pseudo-anonymous identifier <client_id, user_id>.
  void ResetClientId();
#endif  // BUILDFLAG(IS_CHROMEOS)

  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry();

  // Returns the delay before the init tasks (to asynchronously initialize
  // metrics providers) run.
  base::TimeDelta GetInitializationDelay();

  // Returns the delay before the task to update the "last alive timestamp" is
  // run.
  base::TimeDelta GetUpdateLastAliveTimestampDelay();

  MetricsLogStore* LogStoreForTest() {
    return reporting_service_.metrics_log_store();
  }

  // Test hook to safely stage the current log in the log store.
  bool StageCurrentLogForTest();

  MetricsLog* GetCurrentLogForTest() { return current_log_.get(); }

  DelegatingProvider* GetDelegatingProviderForTesting() {
    return &delegating_provider_;
  }

  // Adds/Removes a logs observer. Observers are notified when a log is newly
  // created and is now known by the metrics service. This may occur when
  // closing a log, or when loading a log from persistent storage. Observers are
  // also notified when an event occurs on the log (e.g., log is staged,
  // uploaded, etc.). See MetricsLogsEventManager::LogEvent for more details.
  void AddLogsObserver(MetricsLogsEventManager::Observer* observer);
  void RemoveLogsObserver(MetricsLogsEventManager::Observer* observer);

  MetricsServiceObserver* logs_event_observer() {
    return logs_event_observer_.get();
  }

  // Observers will be notified when the enablement state changes. The callback
  // should accept one boolean argument, which will signal whether or not the
  // metrics collection has been enabled.
  base::CallbackListSubscription AddEnablementObserver(
      const base::RepeatingCallback<void(bool)>& observer);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  bool IsInForegroundForTesting() const { return is_in_foreground_; }
#endif

  // Creates a new MetricsLog instance with the given |log_type|.
  std::unique_ptr<MetricsLog> CreateLogForTesting(
      MetricsLog::LogType log_type) {
    return CreateLog(log_type);
  }

 protected:
  // Sets the persistent system profile. Virtual for tests.
  virtual void SetPersistentSystemProfile(const std::string& serialized_proto,
                                          bool complete);

  // Records the current environment (system profile) in |log|, and persists
  // the results in prefs.
  // Exposed for testing.
  static std::string RecordCurrentEnvironmentHelper(
      MetricsLog* log,
      PrefService* local_state,
      DelegatingProvider* delegating_provider);

  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    CONSTRUCTED,          // Constructor was called.
    INITIALIZED,          // InitializeMetricsRecordingState() was called.
    INIT_TASK_SCHEDULED,  // Waiting for deferred init tasks to finish.
    INIT_TASK_DONE,       // Waiting for timer to send initial log.
    SENDING_LOGS,         // Sending logs and creating new ones when we run out.
  };

  State state() const { return state_; }

 private:
  // The current state of recording for the MetricsService. The state is UNSET
  // until set to something else, at which point it remains INACTIVE or ACTIVE
  // for the lifetime of the object.
  enum RecordingState {
    INACTIVE,
    ACTIVE,
    UNSET,
  };

  // The result of a call to FinalizeLog().
  struct FinalizedLog {
    FinalizedLog();
    ~FinalizedLog();

    // This type is move only.
    FinalizedLog(FinalizedLog&& other);
    FinalizedLog& operator=(FinalizedLog&& other);

    // The size of the uncompressed log data. This is only used for calculating
    // some metrics.
    size_t uncompressed_log_size;

    // A LogInfo object representing the log, which contains its compressed
    // data, hash, signature, timestamp, and some metadata.
    std::unique_ptr<UnsentLogStore::LogInfo> log_info;
  };

  // Writes snapshots of histograms owned by the StatisticsRecorder to a log.
  // Does not take ownership of the log.
  // TODO(crbug.com/40897621): Although this class takes in |required_flags| in
  // its constructor to filter the StatisticsRecorder histograms being put into
  // the log, the |histogram_snapshot_manager_| is not aware of this. So if
  // the |histogram_snapshot_manager_| is passed to some other caller, this
  // caller will need to manually filter the histograms. Re-factor the code so
  // that this is not needed.
  class MetricsLogHistogramWriter {
   public:
    explicit MetricsLogHistogramWriter(MetricsLog* log);

    MetricsLogHistogramWriter(MetricsLog* log,
                              base::HistogramBase::Flags required_flags);

    MetricsLogHistogramWriter(const MetricsLogHistogramWriter&) = delete;
    MetricsLogHistogramWriter& operator=(const MetricsLogHistogramWriter&) =
        delete;

    ~MetricsLogHistogramWriter();

    // Snapshots the deltas of histograms known by the StatisticsRecorder and
    // writes them to the log passed in the constructor. This also marks the
    // samples (the deltas) as logged.
    void SnapshotStatisticsRecorderDeltas();

    // Snapshots the unlogged samples of histograms known by the
    // StatisticsRecorder and writes them to the log passed in the constructor.
    // Note that unlike SnapshotStatisticsRecorderDeltas(), this does not mark
    // the samples as logged. To do so, a call to MarkUnloggedSamplesAsLogged()
    // (in |histogram_snapshot_manager_|) should be made.
    void SnapshotStatisticsRecorderUnloggedSamples();

    base::HistogramSnapshotManager* histogram_snapshot_manager() {
      return histogram_snapshot_manager_.get();
    }

    base::StatisticsRecorder::SnapshotTransactionId snapshot_transaction_id() {
      return snapshot_transaction_id_;
    }

    // Notifies the histogram writer that the `log` passed in through the
    // constructor is about to be destroyed.
    void NotifyLogBeingFinalized();

   private:
    // Used to select which histograms to record when calling
    // SnapshotStatisticsRecorderHistograms() or
    // SnapshotStatisticsRecorderUnloggedSamples().
    const base::HistogramBase::Flags required_flags_;

    // Used to write histograms to the log passed in the constructor. Null after
    // `NotifyLogBeingFinalized()`.
    std::unique_ptr<base::HistogramFlattener> flattener_;

    // Used to snapshot histograms.
    std::unique_ptr<base::HistogramSnapshotManager> histogram_snapshot_manager_;

    // The snapshot transaction ID of a call to either
    // SnapshotStatisticsRecorderDeltas() or
    // SnapshotStatisticsRecorderUnloggedSamples().
    base::StatisticsRecorder::SnapshotTransactionId snapshot_transaction_id_;
  };

  // Loads "independent" metrics from a metrics provider and executes a
  // callback when complete, which could be immediate or after some
  // execution on a background thread.
  class IndependentMetricsLoader {
   public:
    explicit IndependentMetricsLoader(std::unique_ptr<MetricsLog> log,
                                      std::string app_version,
                                      std::string signing_key);

    IndependentMetricsLoader(const IndependentMetricsLoader&) = delete;
    IndependentMetricsLoader& operator=(const IndependentMetricsLoader&) =
        delete;

    ~IndependentMetricsLoader();

    // Call ProvideIndependentMetrics (which may execute on a background thread)
    // for the |metrics_provider| and execute the |done_callback| when complete
    // with the result (true if successful). |done_callback| must own |this|.
    void Run(base::OnceCallback<void(bool)> done_callback,
             MetricsProvider* metrics_provider);

    // Finalizes/serializes |log_|, and stores the result in |finalized_log_|.
    // Should only be called once, after |log_| has been filled.
    void FinalizeLog();

    // Returns whether FinalizeLog() was called.
    bool HasFinalizedLog();

    // Extracts |finalized_log_|. Should be only called once, after
    // FinalizeLog() has been called. No more operations should be done after
    // this.
    FinalizedLog ReleaseFinalizedLog();

   private:
    std::unique_ptr<MetricsLog> log_;
    std::unique_ptr<base::HistogramFlattener> flattener_;
    std::unique_ptr<base::HistogramSnapshotManager> snapshot_manager_;
    bool run_called_ = false;

    // Used for finalizing |log_| in FinalizeLog().
    const std::string app_version_;
    const std::string signing_key_;

    // Stores the result of FinalizeLog().
    FinalizedLog finalized_log_;
    bool finalize_log_called_ = false;
    bool release_finalized_log_called_ = false;
  };

  // Gets the LogStore for UMA logs.
  MetricsLogStore* log_store() {
    return reporting_service_.metrics_log_store();
  }

  // Calls into the client to initialize some system profile metrics.
  void StartInitTask();

  // Callback that moves the state to INIT_TASK_DONE. When this is called, the
  // state should be INIT_TASK_SCHEDULED.
  void FinishedInitTask();

  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // Get the amount of uptime since this process started and since the last
  // call to this function.  Also updates the cumulative uptime metric (stored
  // as a pref) for uninstall.  Uptimes are measured using TimeTicks, which
  // guarantees that it is monotonic and does not jump if the user changes
  // their clock.  The TimeTicks implementation also makes the clock not
  // count time the computer is suspended.
  void GetUptimes(PrefService* pref,
                  base::TimeDelta* incremental_uptime,
                  base::TimeDelta* uptime);

  // Turns recording on or off.
  // DisableRecording() also forces a persistent save of logging state (if
  // anything has been recorded, or transmitted).
  void EnableRecording();
  void DisableRecording();

  // If in_idle is true, sets idle_since_last_transmission to true.
  // If in_idle is false and idle_since_last_transmission_ is true, sets
  // idle_since_last_transmission to false and starts the timer (provided
  // starting the timer is permitted).
  void HandleIdleSinceLastTransmission(bool in_idle);

  // Set up client ID, session ID, etc.
  void InitializeMetricsState();

  // Opens a new log for recording user experience metrics. If |call_providers|
  // is true, OnDidCreateMetricsLog() of providers will be called right after
  // opening the new log.
  void OpenNewLog(bool call_providers = true);

  // Closes out the current log after adding any last information. |async|
  // determines whether finalizing the log will be done in a background thread.
  // |log_stored_callback| will be run (on the main thread) after the finalized
  // log is stored. Note that when |async| is true, the closed log could end up
  // not being stored (see MaybeCleanUpAndStoreFinalizedLog()). Regardless,
  // |log_stored_callback| is still run. Note that currently, there is only
  // support to close one log asynchronously at a time (this should be enforced
  // by the caller).
  void CloseCurrentLog(
      bool async,
      MetricsLogsEventManager::CreateReason reason,
      base::OnceClosure log_stored_callback = base::DoNothing());

  // Stores the |finalized_log| in |log_store()|.
  void StoreFinalizedLog(MetricsLog::LogType log_type,
                         MetricsLogsEventManager::CreateReason reason,
                         base::OnceClosure done_callback,
                         FinalizedLog finalized_log);

  // Calls MarkUnloggedSamplesAsLogged() on |log_histogram_writer| and stores
  // the |finalized_log| (see StoreFinalizedLog()), but only if the
  // StatisticRecorder's last transaction ID is the same as the one from
  // |log_histogram_writer| at the time of calling. See comments in the
  // implementation for more details.
  void MaybeCleanUpAndStoreFinalizedLog(
      std::unique_ptr<MetricsLogHistogramWriter> log_histogram_writer,
      MetricsLog::LogType log_type,
      MetricsLogsEventManager::CreateReason reason,
      base::OnceClosure done_callback,
      FinalizedLog finalized_log);

  // Pushes the text of the current and staged logs into persistent storage.
  void PushPendingLogsToPersistentStorage(
      MetricsLogsEventManager::CreateReason reason);

  // Ensures that scheduler is running, assuming the current settings are such
  // that metrics should be reported. If not, this is a no-op.
  void StartSchedulerIfNecessary();

  // Starts the process of uploading metrics data.
  void StartScheduledUpload();

  // Called by the client via a callback when final log info collection is
  // complete.
  void OnFinalLogInfoCollectionDone();

  // Called via a callback after a periodic ongoing log (created through the
  // MetricsRotationScheduler) was stored in |log_store()|.
  void OnAsyncPeriodicOngoingLogStored();

  // Prepares the initial stability log, which is only logged when the previous
  // run of Chrome crashed.  This log contains any stability metrics left over
  // from that previous run, and only these stability metrics.  It uses the
  // system profile from the previous session.  |prefs_previous_version| is used
  // to validate the version number recovered from the system profile.  Returns
  // true if a log was created.
  bool PrepareInitialStabilityLog(const std::string& prefs_previous_version);

  // Creates a new MetricsLog instance with the given |log_type|.
  std::unique_ptr<MetricsLog> CreateLog(MetricsLog::LogType log_type);

  // Records the current environment (system profile) in |log|, and persists
  // the results in prefs and GlobalPersistentSystemProfile.
  void RecordCurrentEnvironment(MetricsLog* log, bool complete);

  // Handle completion of PrepareProviderMetricsLog which is run as a
  // background task.
  void PrepareProviderMetricsLogDone(
      std::unique_ptr<IndependentMetricsLoader> loader,
      bool success);

  // Record a single independent profile and associated histogram from
  // metrics providers. If this returns true, one was found and there may
  // be more.
  bool PrepareProviderMetricsLog();

  // Records one independent histogram log and then reschedules itself to
  // check for others. The interval is so as to not adversely impact the UI.
  void PrepareProviderMetricsTask();

  // Updates the "last live" browser timestamp and schedules the next update.
  void UpdateLastLiveTimestampTask();

  // Returns whether it is too early to close a log.
  bool IsTooEarlyToCloseLog();

  // Called if this install is detected as cloned.
  void OnClonedInstallDetected();

  // Snapshots histogram deltas using the passed |log_histogram_writer| and then
  // finalizes |log| by calling FinalizeLog(). |log|, |current_app_version| and
  // |signing_key| are used to finalize the log (see FinalizeLog()).
  // Semantically, this is equivalent to SnapshotUnloggedSamplesAndFinalizeLog()
  // followed by MarkUnloggedSamplesAsLogged().
  static FinalizedLog SnapshotDeltasAndFinalizeLog(
      std::unique_ptr<MetricsLogHistogramWriter> log_histogram_writer,
      std::unique_ptr<MetricsLog> log,
      bool truncate_events,
      std::optional<ChromeUserMetricsExtension::RealLocalTime> close_time,
      std::string&& current_app_version,
      std::string&& signing_key);

  // Snapshots unlogged histogram samples using the passed
  // |log_histogram_writer| and then finalizes |log| by calling FinalizeLog().
  // |log|, |current_app_version| and |signing_key| are used to finalize the log
  // (see FinalizeLog()). Note that unlike SnapshotDeltasAndFinalizeLog(), this
  // does not own the passed |log_histogram_writer|, because it should be
  // available to eventually mark the unlogged samples as logged.
  static FinalizedLog SnapshotUnloggedSamplesAndFinalizeLog(
      MetricsLogHistogramWriter* log_histogram_writer,
      std::unique_ptr<MetricsLog> log,
      bool truncate_events,
      std::optional<ChromeUserMetricsExtension::RealLocalTime> close_time,
      std::string&& current_app_version,
      std::string&& signing_key);

  // Finalizes |log| (see MetricsLog::FinalizeLog()). The |signing_key| is used
  // to compute a signature for the log.
  static FinalizedLog FinalizeLog(
      std::unique_ptr<MetricsLog> log,
      bool truncate_events,
      std::optional<ChromeUserMetricsExtension::RealLocalTime> close_time,
      const std::string& current_app_version,
      const std::string& signing_key);

  // Sub-service for uploading logs.
  MetricsReportingService reporting_service_;

  // The log that we are still appending to.
  std::unique_ptr<MetricsLog> current_log_;

  // Used to manage various metrics reporting state prefs, such as client id,
  // low entropy source and whether metrics reporting is enabled. Weak pointer.
  const raw_ptr<MetricsStateManager> state_manager_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  const raw_ptr<MetricsServiceClient> client_;

  // Registered metrics providers.
  DelegatingProvider delegating_provider_;

  raw_ptr<PrefService> local_state_;

  base::ActionCallback action_callback_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  RecordingState recording_state_;

  // Indicate whether test mode is enabled, where the initial log should never
  // be cut, and logs are neither persisted nor uploaded.
  bool test_mode_active_;

  // The progression of states made by the browser are recorded in the following
  // state.
  State state_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // The scheduler for determining when log rotations should happen.
  std::unique_ptr<MetricsRotationScheduler> rotation_scheduler_;

  // Stores the time of the first call to |GetUptimes()|.
  base::TimeTicks first_updated_time_;

  // Stores the time of the last call to |GetUptimes()|.
  base::TimeTicks last_updated_time_;

  // Indicates if loading of independent metrics is currently active.
  bool independent_loader_active_ = false;

  // Indicates whether or not there is currently a periodic ongoing log being
  // finalized (or is scheduled to be finalized).
  bool pending_ongoing_log_ = false;

  // Stores the time when we last posted a task to finalize a periodic ongoing
  // log asynchronously.
  base::TimeTicks async_ongoing_log_posted_time_;

  // Logs event manager to keep track of the various logs that the metrics
  // service interacts with. An unowned pointer of this instance is passed down
  // to various objects that are owned by this class.
  MetricsLogsEventManager logs_event_manager_;

  // An observer that observes all events notified through |logs_event_manager_|
  // since the creation of this MetricsService instance. This is only created
  // if this is a debug build, or the |kExportUmaLogsToFile| command line flag
  // is passed. This is primarily used by the chrome://metrics-internals debug
  // page.
  std::unique_ptr<MetricsServiceObserver> logs_event_observer_;

  // A set of observers that keeps track of the metrics reporting state.
  base::RepeatingCallbackList<void(bool)> enablement_observers_;

  // Subscription for a callback that runs if this install is detected as
  // cloned.
  base::CallbackListSubscription cloned_install_subscription_;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Indicates whether OnAppEnterForeground() (true) or OnAppEnterBackground
  // (false) was called.
  bool is_in_foreground_ = false;
#endif

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ActiveFieldTrialsReported);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, IsPluginProcess);
  FRIEND_TEST_ALL_PREFIXES(::ChromeMetricsServiceClientTest,
                           TestRegisterMetricsServiceProviders);
  FRIEND_TEST_ALL_PREFIXES(::IOSChromeMetricsServiceClientTest,
                           TestRegisterMetricsServiceProviders);
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as MetricsService.
  base::WeakPtrFactory<MetricsService> self_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_H_
