// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/external_metrics.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/metrics/cast_stability_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromecast {
namespace metrics {

namespace {

bool CheckValues(const std::string& name,
                 int minimum,
                 int maximum,
                 size_t bucket_count) {
  if (!base::Histogram::InspectConstructionArguments(
          name, &minimum, &maximum, &bucket_count))
    return false;
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return true;
  return histogram->HasConstructionArguments(minimum, maximum, bucket_count);
}

bool CheckLinearValues(const std::string& name, int maximum) {
  return CheckValues(name, 1, maximum, maximum + 1);
}

// Returns a task runner appropriate for running background tasks that perform
// file I/O.
scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  // Note that CollectEvents accesses a global singleton, and thus
  // scheduling with CONTINUE_ON_SHUTDOWN might not be safe.
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

// The interval between external metrics collections in seconds
static const int kExternalMetricsCollectionIntervalSeconds = 30;

ExternalMetrics::ExternalMetrics(
    CastStabilityMetricsProvider* stability_provider,
    const std::string& uma_events_file)
    : stability_provider_(stability_provider),
      uma_events_file_(uma_events_file),
      task_runner_(CreateTaskRunner()),
      weak_factory_(this) {
  DCHECK(stability_provider);

  // The sequence checker verifies that all of the interesting work done by this
  // class is done on the |task_runner_|, rather than on the sequence that this
  // object was created on.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExternalMetrics::~ExternalMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExternalMetrics::StopAndDestroy() {
  task_runner_->DeleteSoon(FROM_HERE, this);
}

void ExternalMetrics::Start() {
  ScheduleCollection();
}

void ExternalMetrics::ProcessExternalEvents(base::OnceClosure cb) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ExternalMetrics::CollectEvents),
                     weak_factory_.GetWeakPtr()),
      std::move(cb));
}

void ExternalMetrics::RecordCrash(const std::string& crash_kind) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CastStabilityMetricsProvider::LogExternalCrash,
                     base::Unretained(stability_provider_), crash_kind));
}

void ExternalMetrics::RecordSparseHistogram(
    const ::metrics::MetricSample& sample) {
  CHECK_EQ(::metrics::MetricSample::SPARSE_HISTOGRAM, sample.type());
  base::HistogramBase* counter = base::SparseHistogram::FactoryGet(
      sample.name(), base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample.sample());
}

int ExternalMetrics::CollectEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  std::vector<std::unique_ptr<::metrics::MetricSample>> samples;
  ::metrics::SerializationUtils::ReadAndTruncateMetricsFromFile(
      uma_events_file_, &samples);

  for (auto it = samples.begin(); it != samples.end(); ++it) {
    const ::metrics::MetricSample& sample = **it;

    switch (sample.type()) {
      case ::metrics::MetricSample::CRASH:
        RecordCrash(sample.name());
        break;
      case ::metrics::MetricSample::USER_ACTION:
        CastMetricsHelper::GetInstance()->RecordSimpleAction(sample.name());
        break;
      case ::metrics::MetricSample::HISTOGRAM:
        if (!CheckValues(sample.name(), sample.min(), sample.max(),
                         sample.bucket_count())) {
          DLOG(ERROR) << "Invalid histogram: " << sample.name();
          break;
        }
        UMA_HISTOGRAM_CUSTOM_COUNTS_NO_CACHE(sample.name(),
                                             sample.sample(),
                                             sample.min(),
                                             sample.max(),
                                             sample.bucket_count(),
                                             1);
        break;
      case ::metrics::MetricSample::LINEAR_HISTOGRAM:
        if (!CheckLinearValues(sample.name(), sample.max())) {
          DLOG(ERROR) << "Invalid linear histogram: " << sample.name();
          break;
        }
        UMA_HISTOGRAM_ENUMERATION_NO_CACHE(
            sample.name(), sample.sample(), sample.max());
        break;
      case ::metrics::MetricSample::SPARSE_HISTOGRAM:
        RecordSparseHistogram(sample);
        break;
    }
  }

  return samples.size();
}

void ExternalMetrics::CollectEventsAndReschedule() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CollectEvents();
  ScheduleCollection();
}

void ExternalMetrics::ScheduleCollection() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExternalMetrics::CollectEventsAndReschedule,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(kExternalMetricsCollectionIntervalSeconds));
}

}  // namespace metrics
}  // namespace chromecast
