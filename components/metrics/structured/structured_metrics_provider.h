// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/key_data.h"
#include "components/metrics/structured/recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace structured {

class EventsProto;
class ExternalMetrics;

// StructuredMetricsProvider is responsible for filling out the
// |structured_metrics_event| section of the UMA proto. This class should not be
// instantiated except by the ChromeMetricsServiceClient. This class is not
// thread safe and should only be called on the browser UI sequence, because
// calls from the metrics service come on the UI sequence.
//
// Initialization of the StructuredMetricsProvider must wait until a profile is
// added, because state is stored within the profile directory. Initialization
// happens in several steps:
//
// 1. A StructuredMetricsProvider instance is constructed and owned by the
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
// StructuredMetricsProvider::OnRecord via Recorder::Record via
// EventBase::Record. These events are not uploaded immediately, and are cached
// in ready-to-upload form.
//
// On a call to ProvideCurrentSessionData, the cache of unsent logs is added to
// a ChromeUserMetricsExtension for upload, and is then cleared.
class StructuredMetricsProvider : public metrics::MetricsProvider,
                                  public Recorder::RecorderImpl {
 public:
  StructuredMetricsProvider();
  ~StructuredMetricsProvider() override;
  StructuredMetricsProvider(const StructuredMetricsProvider&) = delete;
  StructuredMetricsProvider& operator=(const StructuredMetricsProvider&) =
      delete;

 private:
  friend class Recorder;
  friend class StructuredMetricsProviderTest;
  friend class StructuredMetricsProviderHwidTest;

  // State machine for step 4 of initialization. These are stored in three files
  // that are asynchronously read from disk at startup. When all files have
  // been read, the provider has been initialized.
  enum class InitState {
    kUninitialized = 1,
    // Set after we observe the recorder, which happens on construction.
    kProfileAdded = 2,
    // Set after all key and event files are read from disk.
    kInitialized = 3,
  };

  void OnKeyDataInitialized();
  void OnRead(ReadStatus status);
  void OnWrite(WriteStatus status);
  void OnExternalMetricsCollected(const EventsProto& events);
  void Purge();

  // Recorder::RecorderImpl:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnRecord(const EventBase& event) override;
  void OnReportingStateChanged(bool enabled) override;
  void OnHardwareClassInitialized(
      const std::string& full_hardware_class) override;
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash) override;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(base::OnceCallback<void(bool)> done_callback,
                                 ChromeUserMetricsExtension* uma_proto,
                                 base::HistogramSnapshotManager*) override;

  void WriteNowForTest();
  void SetExternalMetricsDirForTest(const base::FilePath& dir);
  void SetDeviceKeyDataPathForTest(const base::FilePath& path);

  // Records events before |init_state_| is kInitialized.
  void RecordEventBeforeInitialization(const EventBase& event);

  // Hashes data from |event| to be persisted to disk and eventually sent.
  void RecordEventFromEventBase(const EventBase& event);

  // Hashes events and persists the events to disk. Should be called once |this|
  // has been initialized.
  void HashUnhashedEventsAndPersist();

  // Populates full hardware class needed for Structured Metrics.
  // Independent metric uploads must populate hardware_class as the normal
  // ChromeOSMetricsProvider will not be called to populate the SystemProfile.
  void ProvideFullHardwareClass(SystemProfileProto* system_profile);

  // Beyond this number of logging events between successive calls to
  // ProvideCurrentSessionData, we stop recording events.
  static int kMaxEventsPerUpload;

  // The path used to store per-profile keys. Relative to the user's
  // cryptohome. This file is created by chromium.
  static char kProfileKeyDataPath[];

  // The path used to store per-device keys. This file is created by tmpfiles.d
  // on start and has its permissions and ownership set such that it is writable
  // by chronos.
  static char kDeviceKeyDataPath[];

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

  // Store for events that were recorded before user/device keys are loaded.
  std::vector<EventBase> unhashed_events_;

  // Storage for all event's keys, and hashing logic for values. This stores
  // keys on disk. |profile_key_data_| stores keys for per-profile projects,
  // and |device_key_data_| stores keys for per-device projects.
  std::unique_ptr<KeyData> profile_key_data_;
  std::unique_ptr<KeyData> device_key_data_;

  // Used to override the otherwise hardcoded path for device keys in unit tests
  // only.
  absl::optional<base::FilePath> device_key_data_path_for_test_;

  // Hardware class used to populate independent metric uploads.
  absl::optional<std::string> full_hardware_class_;

  base::WeakPtrFactory<StructuredMetricsProvider> weak_factory_{this};
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
