// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_RECORDER_H_

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;

}

// Recorder is a singleton to help communicate with the
// StructuredMetricsProvider. It serves three purposes:
// 1. Begin the initialization of the StructuredMetricsProvider (see class
// comment for more details).
// 2. Add an event for the StructuredMetricsProvider to record.
// 3. Retrieving information about project's key, specifically the day it was
// last rotated.
//
// The StructuredMetricsProvider is owned by the MetricsService, but it needs to
// be accessible to any part of the codebase, via an EventBase subclass, to
// record events. The StructuredMetricsProvider registers itself as an observer
// of this singleton when recording is enabled, and calls to Record (for
// recording) or ProfileAdded (for initialization) are then forwarded to it.
//
// Recorder is embedded within StructuredMetricsClient for Ash Chrome and should
// only be used in Ash Chrome.
//
// TODO(b/282031543): Remove this class and merge remaining logic into
// structured_metrics_recorder.h since the Record() is exposed via
// StructuredMetricsClient interface now.
class Recorder {
 public:
  class RecorderImpl : public base::CheckedObserver {
   public:
    // Called on a call to Record.
    virtual void OnEventRecord(const Event& event) = 0;
    // Called on a call to ProfileAdded.
    virtual void OnProfileAdded(const base::FilePath& profile_path) = 0;
    // Called on a call to OnReportingStateChanged.
    virtual void OnReportingStateChanged(bool enabled) = 0;
    // Called when SystemProfile has finished loading
    virtual void OnSystemProfileInitialized() {}
    // Called on a call to LastKeyRotation.
    virtual absl::optional<int> LastKeyRotation(uint64_t project_name_hash) = 0;
  };

  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  static Recorder* GetInstance();

  // This signals to StructuredMetricsProvider that the event should be
  // recorded.
  void RecordEvent(Event&& event);

  // Notifies the StructuredMetricsProvider that a profile has been added with
  // path |profile_path|. The first call to ProfileAdded initializes the
  // provider using the keys stored in |profile_path|, so care should be taken
  // to ensure the first call provides a |profile_path| suitable for metrics
  // collection.
  // TODO(crbug.com/1016655): When structured metrics expands beyond Chrome OS,
  // investigate whether initialization can be simplified for Chrome.
  void ProfileAdded(const base::FilePath& profile_path);

  // Returns when the key for |event| was last rotated, in days since epoch.
  // Returns nullopt if the information is not available.
  absl::optional<int> LastKeyRotation(const Event& event);

  // Notifies observers that metrics reporting has been enabled or disabled.
  void OnReportingStateChanged(bool enabled);

  // Notifies observers that system profile has been loaded.
  void OnSystemProfileInitialized();

  void SetUiTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(RecorderImpl* observer);
  void RemoveObserver(RecorderImpl* observer);

  // Adds |events_processor| to further add metadata to recorded events or
  // listen to recorded events.
  void AddEventsProcessor(
      std::unique_ptr<EventsProcessorInterface> events_processor);

  // Modifies |uma_proto| before the log is sent.
  void OnProvideIndependentMetrics(ChromeUserMetricsExtension* uma_proto);

 private:
  friend class base::NoDestructor<Recorder>;

  Recorder();
  ~Recorder();

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::ObserverList<RecorderImpl> observers_;

  DelegatingEventsProcessor delegating_events_processor_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
