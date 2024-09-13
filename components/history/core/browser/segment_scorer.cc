// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/segment_scorer.h"

#include <math.h>

#include <utility>

#include "base/memory/ptr_util.h"

namespace history {

/******** SegmentScorer::RecencyFactor ********/

SegmentScorer::RecencyFactor::~RecencyFactor() = default;

/******** SegmentScorer::RecencyFactorDefault ********/

SegmentScorer::RecencyFactorDefault::~RecencyFactorDefault() = default;

// Computes a smooth function that boosts today's visits by 3x, week-ago visits
// by 2x, 3-week-ago visits by 1.5x, falling off to 1x asymptotically.
float SegmentScorer::RecencyFactorDefault::Compute(int days_ago) {
  return 1.0f + (2.0f * (1.0f / (1.0f + days_ago / 7.0f)));
}

/******** SegmentScorer ********/

// static
std::unique_ptr<SegmentScorer> SegmentScorer::CreateFromFeatureFlags() {
  // TODO(crbug.com/365590025): Add feature flags, and use them to configure.
  return base::WrapUnique(new SegmentScorer());
}

SegmentScorer::SegmentScorer() {
  recency_factor_ = std::make_unique<RecencyFactorDefault>();
}

SegmentScorer::~SegmentScorer() = default;

float SegmentScorer::Compute(const std::vector<base::Time>& time_slots,
                             const std::vector<int>& visit_counts,
                             base::Time now) const {
  size_t n = time_slots.size();
  CHECK_EQ(n, visit_counts.size());
  float score = 0.0f;

  for (size_t i = 0; i < n; ++i) {
    base::Time time_slot = time_slots[i];
    int visit_count = visit_counts[i];
    // Score for this day in isolation.
    float day_visits_score = (visit_count <= 0.0f)
                                 ? 0.0f
                                 : 1.0f + log(static_cast<float>(visit_count));
    // Recent visits count more than historical ones, so multiply a
    // recency factor related to how long ago this day was.
    int days_ago = (now - time_slot).InDays();
    score += recency_factor_->Compute(days_ago) * day_visits_score;
  }
  return score;
}

}  // namespace history
