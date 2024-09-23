// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_

#include <atomic>
#include <deque>
#include <memory>
#include <optional>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/lib/event_storage.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/key_data_provider.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/recorder.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

// StructuredMetricsRecorder is responsible for storing and managing the all
// Structured Metrics events recorded on-device.  This class is not thread safe
// and should only be called on the browser UI sequence, because calls from the
// metrics service come on the UI sequence.
//
// This is to be used as a base class for different platform implementations.
// The subclass will instantiate the desired KeyDataProvider and EventStorage.
//
// This class accepts events to record from
// StructuredMetricsRecorder::OnRecord via Recorder::Record via
// Event::Record. These events are not uploaded immediately, and are cached
// in ready-to-upload form.
class StructuredMetricsRecorder
    : public Recorder::RecorderImpl,
      public base::RefCountedDeleteOnSequence<StructuredMetricsRecorder>,
      KeyDataProvider::Observer {
 public:
  // Interface for watching for the recording of Structured Metrics Events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEventRecorded(const StructuredEventProto& event) = 0;
  };

  StructuredMetricsRecorder(
      std::unique_ptr<KeyDataProvider> key_data_provider,
      std::unique_ptr<EventStorage<StructuredEventProto>> event_storage);
  StructuredMetricsRecorder(const StructuredMetricsRecorder&) = delete;
  StructuredMetricsRecorder& operator=(const StructuredMetricsRecorder&) =
      delete;

  // Manages whether or not Structured Metrics is recording.
  // If these functions are overloaded, make sure they are explicitly called in
  // the overriding function.
  virtual void EnableRecording();
  virtual void DisableRecording();

  void Purge();

  bool recording_enabled() const { return recording_enabled_; }

  void ProvideUmaEventMetrics(ChromeUserMetricsExtension& uma_proto);

  // Provides event metrics stored in the recorder into |uma_proto|.
  //
  // This calls OnIndependentMetrics() to populate |uma_proto| with metadata
  // fields.
  virtual void ProvideEventMetrics(ChromeUserMetricsExtension& uma_proto);

  // Provides any additional metadata needed by the UMA proto.
  //
  // This should be called on the UI thread.
  // If this method is overwritten the base implementation must be called.
  virtual void ProvideLogMetadata(ChromeUserMetricsExtension& uma_proto);

  // Returns true if ready to provide metrics via ProvideEventMetrics.
  bool CanProvideMetrics();

  // Returns true if there are metrics to provide.
  bool HasMetricsToProvide();

  // KeyDataProvider::Observer:
  void OnKeyReady() override;

  // Interface for adding and remove watchers.
  void AddEventsObserver(Observer* watcher);
  void RemoveEventsObserver(Observer* watcher);

  EventStorage<StructuredEventProto>* event_storage() {
    return event_storage_.get();
  }

  KeyDataProvider* key_data_provider() { return key_data_provider_.get(); }

 protected:
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsMixin;

  // Recorder::RecorderImpl:
  void OnEventRecord(const Event& event) override;

  // Different initialization states for the recorder.
  enum State {
    kUninitialized,
    // Set once OnKeyReady has been called once.
    kKeyDataInitialized,
    // Set once the profile key data has been initialized.
    kProfileKeyDataInitialized,
    kMaxValue = kProfileKeyDataInitialized,
  };

  // Collection of InitValues that represents the current initialization state
  // of the recorder.
  //
  // For events to be persisted, both kKeyDataInitialized, kEventsInitialized,
  // and kProfileKeyDataInitialized msut be set for events to be recorded.
  using InitState =
      base::EnumSet<State, State::kUninitialized, State::kMaxValue>;

  bool HasState(State state) const;

 private:
  friend class Recorder;
  friend class StructuredMetricsMixin;
  friend class StructuredMetricsProviderTest;
  friend class StructuredMetricsRecorderTest;
  friend class StructuredMetricsRecorderHwidTest;
  friend class TestStructuredMetricsRecorder;
  friend class TestStructuredMetricsProvider;
  friend class StructuredMetricsServiceTest;

  // Records events before IsInitialized().
  void RecordEventBeforeInitialization(const Event& event);

  // Records events before IsProfileInitialized().
  void RecordProfileEventBeforeInitialization(const Event& event);

  // Records |event| to persistent disk to be eventually sent.
  void RecordEvent(const Event& event);

  // Sets the event and project fields and the identification fields.
  void InitializeEventProto(StructuredEventProto* proto,
                            const Event& event,
                            const ProjectValidator& project_validator,
                            const EventValidator& event_validator);

  // Processes the events metric to proto format.
  void AddMetricsToProto(StructuredEventProto* proto,
                         const Event& event,
                         const ProjectValidator& project_validator,
                         const EventValidator& validator);

  // Adds sequence metadata to the event.
  virtual void AddSequenceMetadata(StructuredEventProto* proto,
                                   const Event& event,
                                   const ProjectValidator& project_validator,
                                   const KeyData& key_data) {}

  // Hashes events and persists the events to disk. Should be called once |this|
  // has been initialized.
  void HashUnhashedEventsAndPersist();

  // Checks if |project_name_hash| can be uploaded.
  bool CanUploadProject(uint64_t project_name_hash) const;

  // Builds a cache of disallow projects from the Finch controlled variable.
  void CacheDisallowedProjectsSet();

  // Returns true if key data is ready to use.
  bool IsKeyDataInitialized();

  // Returns true if ready to record events.
  bool IsInitialized();

  // Returns true if ready to record profile events.
  bool IsProfileInitialized();

  // Returns whether the |event| can be recorded event if metrics is opted-out.
  // Note that uploading is still guarded by metrics opt-in state and that these
  // events will never be uploaded. In the event that a user opts-in, these
  // events will be purged.
  bool CanForceRecord(const Event& event) const;

  // Helper functions to determine scope of the event.
  bool IsDeviceEvent(const Event& event) const;
  bool IsProfileEvent(const Event& event) const;

  // Helper function to get the validators for |event|.
  std::optional<std::pair<const ProjectValidator*, const EventValidator*>>
  GetEventValidators(const Event& event) const;

  void SetOnReadyToRecord(base::OnceClosure callback);

  // Sets a callback to be made every time an event is recorded. This is exposed
  // so that tests can check if a specific event is recorded since recording
  // happens asynchronously.
  void SetEventRecordCallbackForTest(base::RepeatingClosure callback);

  // Adds a project to the diallowed list for testing.
  void AddDisallowedProjectForTest(uint64_t project_name_hash);

 protected:
  friend class base::RefCountedDeleteOnSequence<StructuredMetricsRecorder>;
  friend class base::DeleteHelper<StructuredMetricsRecorder>;
  ~StructuredMetricsRecorder() override;

  void NotifyEventRecorded(const StructuredEventProto& event);

  // Key data provider that provides device and profile keys.
  std::unique_ptr<KeyDataProvider> key_data_provider_;

  // Storage for events while on device.
  std::unique_ptr<EventStorage<StructuredEventProto>> event_storage_;

  // Whether the metrics provider has completed initialization. Initialization
  // occurs across OnProfileAdded and OnKeyReady. No incoming
  // events are recorded until initialization has succeeded.
  //
  // Execution is:
  //  - A profile is added.
  //  - OnProfileAdded is called, which constructs |storage_| and
  //    asynchronously reads events and keys are loaded.
  //
  // The metrics provider does not handle multiprofile: initialization happens
  // only once, for the first-logged-in account aka. primary user.
  //
  // After a profile is added, two files need to be read from disk:
  // per-profile keys and per-device keys. |init_count_| tracks
  // how many of these have been read and, when it reaches 2, we set
  // |init_state_| to kInitialized.
  InitState init_state_;

 private:
  // Lock and release event storage. This is to mitigate a potential race
  // condition between TakeEvents() and RecordEvent().
  //
  // If storage is locked then recorded events are stored in-memory until
  // storage is released.
  void LockStorage();
  void ReleaseStorage();

  // Once storage is released then record in-memory events into storage.
  void StoreLockedEvents();

  // Tracks the recording state signalled to the metrics provider by
  // OnRecordingEnabled and OnRecordingDisabled. This is false until
  // OnRecordingEnabled is called, which sets it true if structured metrics'
  // feature flag is enabled.
  bool recording_enabled_ = false;

  // Store for events that were recorded before keys are loaded.
  std::deque<Event> unhashed_events_;

  // Store for events that were recorded before profile keys are loaded.
  std::deque<Event> unhashed_profile_events_;

  // A set of projects that are not allowed to be recorded. This is a cache of
  // GetDisabledProjects().
  base::flat_set<uint64_t> disallowed_projects_;

  base::ObserverList<Observer> watchers_;

  // A flag to determine if the storage has been locked without actually
  // acquiring a lock.
  std::atomic_bool storage_lock_;

  // Events recorded while recording was locked.
  std::vector<StructuredEventProto> locked_events_;

  // Callbacks for tests whenever an event is recorded.
  base::RepeatingClosure test_callback_on_record_ = base::DoNothing();

  // Callback to be made once recorder is ready to persist events to disk.
  base::OnceClosure on_ready_callback_ = base::DoNothing();

  base::WeakPtrFactory<StructuredMetricsRecorder> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_RECORDER_H_
