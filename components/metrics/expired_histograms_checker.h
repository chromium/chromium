// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
#define COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_

#include <stdint.h>

#include <set>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/metrics/record_histogram_checker.h"

namespace metrics {

// ExpiredHistogramsChecker implements RecordHistogramChecker interface
// to avoid recording expired metrics.
class ExpiredHistogramsChecker final : public base::RecordHistogramChecker {
 public:
  // Takes a sorted array of histogram hashes in ascending order and a
  // list of explicitly allowed histogram names as a comma-separated string.
  // Histograms in the |allowlist_str| are logged even if their hash is in the
  // |expired_histograms_hashes|.
  ExpiredHistogramsChecker(base::span<const uint32_t> expired_histogram_hashes,
                           const std::string& allowlist_str);

  ExpiredHistogramsChecker(const ExpiredHistogramsChecker&) = delete;
  ExpiredHistogramsChecker& operator=(const ExpiredHistogramsChecker&) = delete;

  ~ExpiredHistogramsChecker() override;

  // Checks if the given |histogram_hash| corresponds to an expired histogram.
  bool ShouldRecord(uint32_t histogram_hash) const override;

 private:
  // Initializes the |allowlist_| array of histogram hashes that should be
  // recorded regardless of their expiration.
  void InitAllowlist(const std::string& allowlist_str);

  // Array of expired histogram hashes.
  const base::raw_span<const uint32_t> expired_histogram_hashes_;

  // Set of expired histogram hashes that should be recorded.
  std::set<uint32_t> allowlist_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
