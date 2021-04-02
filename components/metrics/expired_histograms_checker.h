// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
#define COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_

#include <stdint.h>
#include <set>

#include "base/macros.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/strings/string_piece.h"

namespace metrics {

// ExpiredHistogramsChecker implements RecordHistogramChecker interface
// to avoid recording expired metrics.
class ExpiredHistogramsChecker final : public base::RecordHistogramChecker {
 public:
  // Takes a sorted array of histogram hashes in ascending order, its size and a
  // list of explicitly allowed histogram names as a comma-separated string.
  // Histograms in the |allowlist_str| are logged even if their hash is in the
  // |expired_histograms_hashes|.
  ExpiredHistogramsChecker(const uint64_t* expired_histogram_hashes,
                           size_t size,
                           const std::string& allowlist_str);
  ~ExpiredHistogramsChecker() override;

  // Checks if the given |histogram_hash| corresponds to an expired histogram.
  bool ShouldRecord(uint64_t histogram_hash) const override;

 private:
  // Initializes the |allowlist_| array of histogram hashes that should be
  // recorded regardless of their expiration.
  void InitAllowlist(const std::string& allowlist_str);

  // Array of expired histogram hashes.
  const uint64_t* const expired_histogram_hashes_;

  // Size of the |expired_histogram_hashes_|.
  const size_t size_;

  // Set of expired histogram hashes that should be recorded.
  std::set<uint64_t> allowlist_;

  DISALLOW_COPY_AND_ASSIGN(ExpiredHistogramsChecker);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
