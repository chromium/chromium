// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_TIMED_EVENT_RECORDER_H_
#define CHROMECAST_METRICS_TIMED_EVENT_RECORDER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromecast {

class MetricsRecorder;

// Helper class to measure and record the time until a named event. The class
// must be destroyed on the same sequence it was created on, but is otherwise
// thread safe.
class TimedEventRecorder final {
 public:
  explicit TimedEventRecorder(MetricsRecorder* metrics_recorder);
  ~TimedEventRecorder();

  TimedEventRecorder(const TimedEventRecorder&) = delete;
  TimedEventRecorder& operator=(const TimedEventRecorder&) = delete;

  // Start measuring the time until RecordEvent(|event_name|). When the event
  // occurs, a metrics event will be recorded with the name |measurement_name|
  // and the elapsed time in milliseconds.
  void MeasureTimeUntilEvent(const std::string& event_name,
                             const std::string& measurement_name,
                             base::TimeTicks now);

  // Make a record of |event_name| and handle any matching ongoing measurements.
  void RecordEvent(const std::string& event_name, base::TimeTicks now);

 private:
  struct TimelineMeasurement {
    std::string name;
    base::TimeTicks start_time;
  };

  MetricsRecorder* const metrics_recorder_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Maps a named event to a set of TimelineMeasurements that will conclude with
  // that event.
  base::flat_map<std::string /* event_name */, std::vector<TimelineMeasurement>>
      event_name_to_measurements_;

  base::WeakPtrFactory<TimedEventRecorder> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_TIMED_EVENT_RECORDER_H_
