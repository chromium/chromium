// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_

#include <deque>
#include <memory>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/external_metrics.h"
#include "components/metrics/structured/key_data_provider.h"
#include "components/metrics/structured/recorder.h"

namespace metrics::structured {

// StructuredMetricsRecorder is responsible for storing and managing the all
// Structured Metrics events recorded on-device.  This class is not thread safe
// and should only be called on the browser UI sequence, because calls from the
// metrics service come on the UI sequence.
//
// Initialization happens in several steps:
//
// 1. A StructuredMetricsRecorder instance is constructed and owned by the
//    StructuredMetricsService. It registers itself as an observer of
//    metrics::structured::Recorder.
//
// 2. Key data and events data are loaded from local state. Once both have been
//    loaded, the recorder is ready to record events.
//
// 3. When a profile is added that is eligible for recording,
//    ChromeMetricsServiceClient calls Recorder::ProfileAdded, which notifies
//    this class. Key data and events data are loaded from the profile's
//    directory.
//
// After step 2, this class accepts events to record from
// StructuredMetricsRecorder::OnRecord via Recorder::Record via Event::Record.
// These events are not uploaded immediately, and are cached in ready-to-upload
// form.
//
// After a profile is loaded, all subsequent events recorded will be stored in
// the profile directory.
//
// On a call to ProvideIndependentMetrics, the cache of unsent logs is added to
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

  bool can_provide_local_state_metrics() const {
    return recording_enabled() && IsReadyToRecordLocalStateEvents();
  }

  bool can_provide_profile_metrics() const {
    return recording_enabled() && IsReadyToRecordProfileEvents();
  }

  bool HasIndependentMetrics();

  bool IsReadyToRecordLocalStateEvents() const;
  bool IsReadyToRecordProfileEvents() const;

  // Returns pointer to in-memory events.
  EventsProto* LocalStateEvents();
  EventsProto* ProfileEvents();

 protected:
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsMixin;

  // Should only be used for tests.
  //
  // TODO(crbug/1350322): Use this ctor to replace existing ctor.
  StructuredMetricsRecorder(base::TimeDelta write_delay,
                            metrics::MetricsProvider* system_profile_provider,
                            const base::FilePath& device_events_store_path);

  PersistentProto<EventsProto>& local_state_events_proto() {
    return *local_state_events_.get();
  }

  PersistentProto<EventsProto>& profile_events_proto() {
    return *profile_events_.get();
  }

 private:
  friend class Recorder;
  friend class StructuredMetricsMixin;
  friend class StructuredMetricsProviderTest;
  friend class StructuredMetricsRecorderTest;
  friend class StructuredMetricsRecorderHwidTest;
  friend class TestStructuredMetricsRecorder;
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsServiceTest;

  // Different initialization state for the recorder.
  enum InitValue {
    kUninitialized,
    // Set once local state keys has been loaded.
    kLocalStateKeyDataInitialized,
    // Set once persistent storage of local state events has been loaded.
    kLocalStateEventsDataLoaded,
    // Set once a profile has been added.
    kProfileAdded,
    // Set once device key has been loaded.
    kProfileKeyDataInitialized,
    // Set once persistent storage of device events has been loaded.
    kProfileEventsDataLoaded,
    kMaxValue = kProfileEventsDataLoaded,
  };

  // Collection of InitValues that represents the current initialization state
  // of the recorder.
  //
  // For events to be persisted, both k{LocalState|Profile|}KeyDataInitialized
  // and k{LocalState|Profile|}EventsDataLoaded must be set for events to be
  // stored in the local state or profile store, respectively.
  using InitState =
      base::EnumSet<InitValue, InitValue::kUninitialized, InitValue::kMaxValue>;

  void OnLocalStateKeyDataInitialized();
  void OnProfileKeyDataInitialized();

  void OnLocalStateEventsRead(ReadStatus status);
  void OnProfileEventsRead(ReadStatus status);

  // Callbacks for reading,writing PersistentProto events respectively.
  void LogReadStatus(ReadStatus status);
  void LogWriteStatus(WriteStatus status);

  // Adds the external metrics collected into |device_events_|.
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
  void SetLocalStateMetricsPathForTest(const base::FilePath& path);
  void SetLocalStateKeysPathForTest(const base::FilePath& path);

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

  bool IsLocalStateKeyDataInitialized();
  bool IsProfileKeyDataInitialized();

  // Beyond this number of logging events between successive calls to
  // ProvideCurrentSessionData, we stop recording events.
  static int kMaxEventsPerUpload;

  // Path to store persistent device events.
  static char kLocalStateEventsPath[];

  // The directory used to store unsent logs. Relative to the user's cryptohome.
  // This file is created by chromium.
  static char kUnsentLogsPath[];

  // Whether the metrics provider has completed initialization. Initialization
  // states are captured in the InitState enum.
  //
  // For the recorder to be ready, both kDeviceKeyDataInitialized and
  // kLocalStateEventsDataLoaded need to be true. These fields are flipped to
  // true when the key data and events data have been loaded from local state,
  // respectively.
  //
  // After a profile is added, two files need to be read from disk:
  // per-profile keys and unsent events. kProfileKeyDataInitialized and
  // kProfileEventsDataLoaded will be flipped to true when the data is loaded,
  // respectively.
  //
  // The metrics provider does not handle multiprofile: initialization happens
  // only once, for the first-logged-in account aka. primary user.
  InitState init_state_;

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

  // On-device storage to hold events before a profile has been added. This
  // storage will be used until the profile events have been initialized.
  //
  // TODO(b/304586233): Once events from |local_state_events_| are staged to be
  // uploaded, this pointer is no longer needed once a user has logged in.
  std::unique_ptr<PersistentProto<EventsProto>> local_state_events_;

  // On-device storage for unsent logs that are collected after a profile has
  // been added. This storage will be used once the profile events and key have
  // been initialized.
  std::unique_ptr<PersistentProto<EventsProto>> profile_events_;

  // Key data provider that provides device and profile keys.
  std::unique_ptr<KeyDataProvider> key_data_provider_;

  // Store for events that were recorded before device keys are loaded. These
  // events will be flushed to persistent storage once the device keys are
  // loaded.
  //
  // If a user event is recorded before a user has logged in, then it will be
  // dropped.
  std::deque<Event> unhashed_events_;

  // Whether the system profile has been initialized.
  bool system_profile_initialized_ = false;

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

  // Path where local state events will be stored.
  base::FilePath local_state_events_store_path_;

  base::WeakPtrFactory<StructuredMetricsRecorder> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
