// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_

#include <deque>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/external_metrics.h"
#include "components/metrics/structured/key_data.h"
#include "components/metrics/structured/key_data_provider.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/recorder.h"

namespace metrics::structured {

// StructuredMetricsRecorder is responsible for storing and managing the all
// Structured Metrics events recorded on-device.  This class is not thread safe
// and should only be called on the browser UI sequence, because calls from the
// metrics service come on the UI sequence.
//
// Initialization of the StructuredMetricsRecorder must wait until a profile is
// added, because state is stored within the profile directory. Initialization
// happens in several steps:
//
// 1. A StructuredMetricsRecorder instance is constructed and owned by the
//    MetricsService. It registers itself as an observer of
//    metrics::structured::Recorder.
//
// 2. When a profile is added that is eligible for recording,
//    ChromeMetricsServiceClient calls Recorder::ProfileAdded, which notifies
//    this class.
//
// 3. This class then begins initialization by asynchronously reading keys and
//    unsent logs from the cryptohome.
//
// 4. If the read succeeds, initialization is complete and this class starts
//    accepting events to record.
//
// After initialization, this class accepts events to record from
// StructuredMetricsRecorder::OnRecord via Recorder::Record via
// Event::Record. These events are not uploaded immediately, and are cached
// in ready-to-upload form.
//
// On a call to ProvideUmaEventMetrics, the cache of unsent logs is added to
// a ChromeUserMetricsExtension for upload, and is then cleared.
class StructuredMetricsRecorder : public Recorder::RecorderImpl {
 public:
  explicit StructuredMetricsRecorder(
      metrics::MetricsProvider* system_profile_provider);
  ~StructuredMetricsRecorder() override;
  StructuredMetricsRecorder(const StructuredMetricsRecorder&) = delete;
  StructuredMetricsRecorder& operator=(const StructuredMetricsRecorder&) =
      delete;

  virtual void EnableRecording();
  virtual void DisableRecording();

  void Purge();

  bool recording_enabled() const { return recording_enabled_; }

  void ProvideUmaEventMetrics(ChromeUserMetricsExtension& uma_proto);

  // Provides event metrics stored in the recorder into |uma_proto|.
  //
  // This calls OnIndependentMetrics() to populate |uma_proto| with metadata
  // fields.
  void ProvideEventMetrics(ChromeUserMetricsExtension& uma_proto);

  void InitializeKeyDataProvider(
      std::unique_ptr<KeyDataProvider> key_data_provider);

  bool can_provide_metrics() const {
    return recording_enabled() && is_init_state(InitState::kInitialized);
  }

  // Returns pointer to in-memory events.
  EventsProto* events() { return events_->get(); }

 protected:
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsMixin;

  // Should only be used for tests.
  //
  // TODO(crbug/1350322): Use this ctor to replace existing ctor.
  StructuredMetricsRecorder(base::TimeDelta write_delay,
                            metrics::MetricsProvider* system_profile_provider);

  PersistentProto<EventsProto>& proto() { return *events_.get(); }

 private:
  friend class Recorder;
  friend class StructuredMetricsMixin;
  friend class StructuredMetricsProviderTest;
  friend class StructuredMetricsRecorderTest;
  friend class StructuredMetricsRecorderHwidTest;
  friend class TestStructuredMetricsRecorder;
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsServiceTest;

  // files that are asynchronously read from disk at startup. When all files
  // have been read, the provider has been initialized.
  enum class InitState {
    kUninitialized = 1,
    // Set once InitializeKeyDataProvider has been called.
    kKeyDataInitialized = 2,
    // Set after we observe the recorder, which happens on construction.
    kProfileAdded = 3,
    // Set after all key and event files are read from disk.
    kInitialized = 4,
  };

  bool is_init_state(InitState state) const { return init_state_ == state; }

  void OnKeyDataInitialized();

  void OnRead(ReadStatus status);
  void OnWrite(WriteStatus status);
  void OnExternalMetricsCollected(const EventsProto& events);

  // Recorder::RecorderImpl:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnEventRecord(const Event& event) override;
  void OnReportingStateChanged(bool enabled) override;
  void OnSystemProfileInitialized() override;
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash) override;

  void WriteNowForTest();
  void SetExternalMetricsDirForTest(const base::FilePath& dir);
  void SetOnReadyToRecord(base::OnceClosure callback);

  // Sets a callback to be made every time an event is recorded. This is exposed
  // so that tests can check if a specific event is recorded since recording
  // happens asynchronously.
  void SetEventRecordCallbackForTest(base::RepeatingClosure callback);

  // Records events before |init_state_| is kInitialized.
  void RecordEventBeforeInitialization(const Event& event);

  // Records |event| to persistent disk to be eventually sent.
  void RecordEvent(const Event& event);

  // Populates system profile needed for Structured Metrics.
  // Independent metric uploads will rely on a SystemProfileProvider
  // to supply the system profile since ChromeOSMetricsProvider will
  // not be called to populate the SystemProfile.
  void ProvideSystemProfile(SystemProfileProto* system_profile);

  // Hashes events and persists the events to disk. Should be called once |this|
  // has been initialized.
  void HashUnhashedEventsAndPersist();

  // Checks if |project_name_hash| can be uploaded.
  bool CanUploadProject(uint64_t project_name_hash) const;

  // Builds a cache of disallow projects from the Finch controlled variable.
  void CacheDisallowedProjectsSet();

  // Adds a project to the diallowed list for testing.
  void AddDisallowedProjectForTest(uint64_t project_name_hash);

  bool IsDeviceKeyDataInitialized();
  bool IsProfileKeyDataInitialized();

  // Increments |init_count_| and checks if the recorder is ready.
  void UpdateAndCheckInitState();

  // Beyond this number of logging events between successive calls to
  // ProvideCurrentSessionData, we stop recording events.
  static int kMaxEventsPerUpload;

  // The directory used to store unsent logs. Relative to the user's cryptohome.
  // This file is created by chromium.
  static char kUnsentLogsPath[];

  // Whether the metrics provider has completed initialization. Initialization
  // occurs across OnProfileAdded and OnInitializationCompleted. No incoming
  // events are recorded until initialization has succeeded.
  //
  // Execution is:
  //  - A profile is added.
  //  - OnProfileAdded is called, which constructs |storage_| and
  //    asynchronously reads events and keys.
  //  - OnInitializationCompleted is called once reading from disk is complete,
  //    which sets |init_count_| to kInitialized.
  //
  // The metrics provider does not handle multiprofile: initialization happens
  // only once, for the first-logged-in account aka. primary user.
  //
  // After a profile is added, three files need to be read from disk:
  // per-profile keys, per-device keys, and unsent events. |init_count_| tracks
  // how many of these have been read and, when it reaches 3, we set
  // |init_state_| to kInitialized.
  InitState init_state_ = InitState::kUninitialized;
  int init_count_ = 0;
  static constexpr int kTargetInitCount = 3;

  // Tracks the recording state signalled to the metrics provider by
  // OnRecordingEnabled and OnRecordingDisabled. This is false until
  // OnRecordingEnabled is called, which sets it true if structured metrics'
  // feature flag is enabled.
  bool recording_enabled_ = false;

  // Set by OnReportingStateChanged if all keys and events should be deleted,
  // but the files backing that state haven't been initialized yet. If set,
  // state will be purged upon initialization.
  bool purge_state_on_init_ = false;

  // The last time we provided independent metrics.
  base::Time last_provided_independent_metrics_;

  // Periodically reports metrics from cros.
  std::unique_ptr<ExternalMetrics> external_metrics_;

  // On-device storage within the user's cryptohome for unsent logs.
  std::unique_ptr<PersistentProto<EventsProto>> events_;

  // Key data provider that provides device and profile keys.
  std::unique_ptr<KeyDataProvider> key_data_provider_;

  // Store for events that were recorded before user/device keys are loaded.
  std::deque<Event> unhashed_events_;

  // Whether the system profile has been initialized.
  bool system_profile_initialized_ = false;

  // File path where device keys will be persisted.
  const base::FilePath device_key_path_;

  // Delay period for PersistentProto writes. Default value of 1000 ms used if
  // not specified in ctor.
  base::TimeDelta write_delay_;

  // Interface for providing the SystemProfile to metrics.
  // See chrome/browser/metrics/chrome_metrics_service_client.h
  raw_ptr<metrics::MetricsProvider, DanglingUntriaged> system_profile_provider_;

  // A set of projects that are not allowed to be recorded. This is a cache of
  // GetDisabledProjects().
  base::flat_set<uint64_t> disallowed_projects_;

  // Callbacks for tests whenever an event is recorded.
  base::RepeatingClosure test_callback_on_record_ = base::DoNothing();

  // The number of scans of external metrics that occurred since the last
  // upload. This is only incremented if events were added by the scan.
  int external_metrics_scans_ = 0;

  // Callback to be made once recorder is ready to persist events to disk.
  base::OnceClosure on_ready_callback_ = base::DoNothing();

  base::WeakPtrFactory<StructuredMetricsRecorder> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
