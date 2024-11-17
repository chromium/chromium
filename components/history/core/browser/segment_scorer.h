// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SEGMENT_SCORER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SEGMENT_SCORER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"

namespace history {

// Scoring function for a history database segment.
class SegmentScorer {
 private:
  // Formula to add more weight to recent visits, and less to past ones.
  struct RecencyFactor {
    static std::unique_ptr<RecencyFactor> CreateFromFeatureFlags();

    virtual ~RecencyFactor();
    virtual float Compute(int days_ago) = 0;
  };

  struct RecencyFactorDefault : public RecencyFactor {
    ~RecencyFactorDefault() override;
    float Compute(int days_ago) override;
  };

  struct RecencyFactorDecay : public RecencyFactor {
    // Requires 0 < |decay_per_day| <= 1.
    explicit RecencyFactorDecay(double decay_per_day);
    ~RecencyFactorDecay() override;
    float Compute(int days_ago) override;

   private:
    const double decay_per_day_;
  };

  struct RecencyFactorDecayStaircase : public RecencyFactor {
    ~RecencyFactorDecayStaircase() override;
    float Compute(int days_ago) override;
  };

 public:
  static std::unique_ptr<SegmentScorer> CreateFromFeatureFlags();

 private:
  SegmentScorer(std::unique_ptr<RecencyFactor> recency_factor,
                int daily_visit_count_cap);

 public:
  ~SegmentScorer();

  SegmentScorer(const SegmentScorer&) = delete;
  const SegmentScorer& operator=(const SegmentScorer&) = delete;

  float Compute(const std::vector<base::Time>& time_slots,
                const std::vector<int>& visit_counts,
                base::Time now) const;

 private:
  std::unique_ptr<RecencyFactor> recency_factor_;
  // Cap for daily visit to prevent domination by single-day outliers.
  int daily_visit_count_cap_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SEGMENT_SCORER_H_
