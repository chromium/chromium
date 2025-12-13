// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/library_support/histogram_manager.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/thread_annotations.h"
#include "components/metrics/histogram_encoder.h"

namespace metrics {

HistogramManager::HistogramManager() : histogram_snapshot_manager_(this) {}

HistogramManager::~HistogramManager() = default;

// static
HistogramManager* HistogramManager::GetInstance() {
  static base::NoDestructor<HistogramManager> histogram_manager;
  return histogram_manager.get();
}

void HistogramManager::RecordDelta(const base::HistogramBase& histogram,
                                   const base::HistogramSamples& snapshot) {
  EncodeHistogramDelta(histogram.histogram_name(), snapshot,
                       uma_proto_.add_histogram_event());
}

// TODO(lukasza): https://crbug.com/881903: NO_THREAD_SAFETY_ANALYSIS below can
// be removed once base::Lock::Try is annotated with EXCLUSIVE_TRYLOCK_FUNCTION.
bool HistogramManager::GetDeltas(std::vector<uint8_t>* data)
    NO_THREAD_SAFETY_ANALYSIS {
  if (get_deltas_lock_.Try()) {
    base::AutoLock lock(get_deltas_lock_, base::AutoLock::AlreadyAcquired());
    // Clear the protobuf between calls.
    uma_proto_.Clear();
    // "false" indicates to *not* include histograms held in persistent storage
    // on the assumption that they will be visible to the recipient through
    // other means.
    base::StatisticsRecorder::PrepareDeltas(
        false, base::Histogram::kNoFlags,
        base::Histogram::kUmaTargetedHistogramFlag,
        &histogram_snapshot_manager_);
    int32_t data_size = uma_proto_.ByteSizeLong();
    data->resize(data_size);
    if (data_size == 0 || uma_proto_.SerializeToArray(data->data(), data_size))
      return true;
  }
  data->clear();
  return false;
}

}  // namespace metrics
