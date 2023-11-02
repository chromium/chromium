// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_histogram_flattener.h"

#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/statistics_recorder.h"

namespace chromecast {
namespace {

// This is an ephemeral utility class for interacting with StatisticsRecorder
// and HistogramSnapshotManager.  It collects histogram samples via
// PrepareDeltas and then puts them in cast::metrics::Histogram form.
class CastRuntimeHistogramFlattener final : public base::HistogramFlattener {
 public:
  CastRuntimeHistogramFlattener() : snapshot_manager_(this) {}
  ~CastRuntimeHistogramFlattener() override = default;

  CastRuntimeHistogramFlattener(const CastRuntimeHistogramFlattener&) = delete;
  CastRuntimeHistogramFlattener& operator=(
      const CastRuntimeHistogramFlattener&) = delete;

  std::vector<cast::metrics::Histogram> GetDeltas() {
    DCHECK(deltas_.empty());

    // Gather all histogram deltas, which will be sent to RecordDelta() and
    // buffered in |deltas_|.
    base::StatisticsRecorder::PrepareDeltas(
        // Only return in-memory/non-persisted histograms.
        false,
        // Do not set flags on histograms.
        base::Histogram::kNoFlags,
        // Only upload metrics marked for UMA upload.
        base::Histogram::kUmaTargetedHistogramFlag, &snapshot_manager_);

    return std::move(deltas_);
  }

 private:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& samples) override {
    DCHECK_NE(0, samples.TotalCount());

    cast::metrics::Histogram converted;
    converted.set_name(histogram.histogram_name());
    converted.set_sum(samples.sum());

    for (std::unique_ptr<base::SampleCountIterator> it = samples.Iterator();
         !it->Done(); it->Next()) {
      base::Histogram::Sample min;
      int64_t max = 0;
      base::Histogram::Count count;
      it->Get(&min, &max, &count);

      cast::metrics::HistogramBucket* bucket = converted.add_bucket();
      bucket->set_min(min);
      bucket->set_max(max);
      bucket->set_count(count);
    }

    deltas_.push_back(std::move(converted));
  }

  base::HistogramSnapshotManager snapshot_manager_;
  std::vector<cast::metrics::Histogram> deltas_;
};

}  // namespace

std::vector<cast::metrics::Histogram> GetHistogramDeltas() {
  return CastRuntimeHistogramFlattener().GetDeltas();
}

}  // namespace chromecast
