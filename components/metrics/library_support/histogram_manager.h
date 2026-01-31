// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LIBRARY_SUPPORT_HISTOGRAM_MANAGER_H_
#define COMPONENTS_METRICS_LIBRARY_SUPPORT_HISTOGRAM_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/metrics/histogram_snapshot_manager.h"
#include "base/synchronization/lock.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

// A HistogramManager instance is created by the app. It is the central
// controller for the acquisition of log data, and recording deltas for
// transmission to an external server. Public APIs are all thread-safe.
class HistogramManager : public base::HistogramSnapshotManager {
 public:
  HistogramManager() = default;

  HistogramManager(const HistogramManager&) = delete;
  HistogramManager& operator=(const HistogramManager&) = delete;

  ~HistogramManager() override;

  // Snapshot all histograms to record the delta into |uma_proto_| and then
  // returns the serialized protobuf representation of the record in |data|.
  // Returns true if it was successfully serialized.
  bool GetDeltas(std::vector<uint8_t>* data);

  // TODO(mef): Hang Histogram Manager off java object instead of singleton.
  static HistogramManager* GetInstance();

 private:
  // base::HistogramSnapshotManager:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override;

  // Stores the protocol buffer representation for this log.
  metrics::ChromeUserMetricsExtension uma_proto_;

  // Should be acquired whenever GetDeltas() is executing to maintain
  // thread-safety.
  base::Lock get_deltas_lock_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_LIBRARY_SUPPORT_HISTOGRAM_MANAGER_H_
