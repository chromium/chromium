// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_legacymetrics/legacymetrics_histogram_flattener.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"

namespace fuchsia_legacymetrics {
namespace {

// Serializes changes to histogram metrics as FIDL structs.
// Cannot be used in conjunction with other metrics collection systems (e.g.
// UMA).
class LegacyMetricsHistogramFlattener : public base::HistogramFlattener {
 public:
  LegacyMetricsHistogramFlattener() : histogram_snapshot_manager_(this) {}
  ~LegacyMetricsHistogramFlattener() override = default;

  LegacyMetricsHistogramFlattener(const LegacyMetricsHistogramFlattener&) =
      delete;
  LegacyMetricsHistogramFlattener& operator=(
      const LegacyMetricsHistogramFlattener&) = delete;

  // Returns a vector of changes to histogram data made since the last call to
  // this method. Returns all histogram data when invoked for the first time.
  std::vector<fuchsia::legacymetrics::Histogram> GetDeltas() {
    DCHECK(histogram_deltas_.empty());

    // Gather all histogram deltas, which will be sent to RecordDelta() and
    // buffered in |histogram_deltas_|.
    base::StatisticsRecorder::PrepareDeltas(
        // Only return in-memory/non-persisted histograms.
        false,
        // Do not set flags on histograms.
        base::Histogram::kNoFlags,
        // Only upload metrics marked for UMA upload.
        base::Histogram::kUmaTargetedHistogramFlag,
        &histogram_snapshot_manager_);

    return std::move(histogram_deltas_);
  }

 private:
  // base::HistogramFlattener implementation.
  // Appends the contents of |snapshot| for the specified |histogram| within
  // |histogram_deltas_|.
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    DCHECK_NE(0, snapshot.TotalCount());

    DVLOG(3) << "RecordDelta " << histogram.histogram_name();

    fuchsia::legacymetrics::Histogram converted;
    converted.set_name(histogram.histogram_name());
    converted.set_sum(snapshot.sum());

    for (std::unique_ptr<base::SampleCountIterator> it = snapshot.Iterator();
         !it->Done(); it->Next()) {
      base::Histogram::Sample min;
      int64_t max = 0;
      base::Histogram::Count count;
      it->Get(&min, &max, &count);

      fuchsia::legacymetrics::HistogramBucket bucket;
      bucket.min = min;
      bucket.max = max;
      bucket.count = count;

      DVLOG(4) << "  Bucket: [" << min << "," << max << ") = " << count;
      converted.mutable_buckets()->emplace_back(std::move(bucket));
    }

    histogram_deltas_.emplace_back(std::move(converted));
  }

  base::HistogramSnapshotManager histogram_snapshot_manager_;
  std::vector<fuchsia::legacymetrics::Histogram> histogram_deltas_;
};

}  // namespace

std::vector<fuchsia::legacymetrics::Histogram> GetLegacyMetricsDeltas() {
  return LegacyMetricsHistogramFlattener().GetDeltas();
}

}  // namespace fuchsia_legacymetrics
