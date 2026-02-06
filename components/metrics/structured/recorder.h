// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_RECORDER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/delegating_events_processor.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/events_processor_interface.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;

}

// Recorder is a thread-safe singleton for recording structured metrics events.
// While RecordEvent() can be called from any thread, it will post the task
// to the UI thread to actually process the event with the registered
// RecorderImpl.
//
// TODO(b/282031543): Remove this class and merge remaining logic into
// structured_metrics_recorder.h since the Record() is exposed via
// StructuredMetricsClient interface now.
//
// TODO(b/339914565): Move recording off of the UI sequence onto an IO sequence.
class Recorder {
 public:
  class RecorderImpl {
   public:
    // Called on a call to Record.
    virtual void OnEventRecord(const Event& event) = 0;
    // Called when SystemProfile has finished loading
    virtual void OnSystemProfileInitialized() {}
  };

  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  static Recorder* GetInstance();

  // Records an event. This method is thread-safe.
  // If not called on the UI thread, this method will post the event
  // processing to the UI thread. The actual recording is handled by
  // the registered RecorderImpl instance on the UI thread.
  void RecordEvent(Event&& event);

  // Notifies observers that system profile has been loaded.
  void OnSystemProfileInitialized();

  void SetUiTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  base::SequencedTaskRunner* GetUiTaskRunner() { return ui_task_runner_.get(); }

  void SetRecorder(RecorderImpl* recorder);
  void UnsetRecorder(RecorderImpl* recorder);

  // Adds |events_processor| to further add metadata to recorded events or
  // listen to recorded events.
  void AddEventsProcessor(
      std::unique_ptr<EventsProcessorInterface> events_processor);

  // Modifies |uma_proto| before the log is sent.
  void OnProvideIndependentMetrics(ChromeUserMetricsExtension* uma_proto);

  // Modifies |event| once after the proto has been built.
  void OnEventRecorded(StructuredEventProto* event);

 private:
  friend class base::NoDestructor<Recorder>;

  Recorder();
  ~Recorder();

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  raw_ptr<RecorderImpl> recorder_;

  DelegatingEventsProcessor delegating_events_processor_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
