// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_
#define CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"

namespace metrics {
class MetricSample;
}  // namespace metrics

namespace chromecast {
namespace metrics {

class CastStabilityMetricsProvider;

// ExternalMetrics service allows processes outside of the Chromecast browser
// process to upload metrics via reading/writing to a known shared file.
class ExternalMetrics {
 public:
  explicit ExternalMetrics(CastStabilityMetricsProvider* stability_provider,
                           const std::string& uma_events_file);

  ExternalMetrics(const ExternalMetrics&) = delete;
  ExternalMetrics& operator=(const ExternalMetrics&) = delete;

  // Begins external data collection. Calls to RecordAction originate in the
  // File thread but are executed in the UI thread.
  void Start();

  // Destroys itself in appropriate thread.
  void StopAndDestroy();

  // Processes all events from shared file. This should be used to consume all
  // events in the file before shutdown. This function is safe to call from any
  // thread.
  void ProcessExternalEvents(base::OnceClosure cb);

 private:
  friend class base::DeleteHelper<ExternalMetrics>;

  ~ExternalMetrics();

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 1024;  // be generous

  // Records an external crash of the given string description.
  void RecordCrash(const std::string& crash_kind);

  // Records a sparse histogram. |sample| is expected to be a sparse histogram.
  void RecordSparseHistogram(const ::metrics::MetricSample& sample);

  // Collects external events from metrics log file.  This is run at periodic
  // intervals.
  //
  // Returns the number of events collected.
  int CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a future collection.
  void ScheduleCollection();

  // Reference to stability metrics provider, for reporting external crashes.
  CastStabilityMetricsProvider* const stability_provider_;

  // File used by libmetrics to send metrics to the browser process.
  const std::string uma_events_file_;

  // The task runner used for running background tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExternalMetrics> weak_factory_;
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_
