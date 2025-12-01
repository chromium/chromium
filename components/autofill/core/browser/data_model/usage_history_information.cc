// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/usage_history_information.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

UsageHistoryInformation::UsageHistoryInformation(size_t usage_history_size)
    : usage_history_size_(usage_history_size), use_dates_(usage_history_size_) {
  CHECK_NE(usage_history_size_, 0u);
  set_use_date(AutofillClock::Now());
  set_modification_date(AutofillClock::Now());
}

UsageHistoryInformation::~UsageHistoryInformation() = default;
UsageHistoryInformation::UsageHistoryInformation(
    const UsageHistoryInformation&) = default;

int UsageHistoryInformation::GetDaysSinceLastUse(
    base::Time current_time) const {
  return current_time <= use_date() ? 0 : (current_time - use_date()).InDays();
}

std::optional<base::Time> UsageHistoryInformation::use_date(size_t i) const {
  CHECK(1 <= i && i <= usage_history_size());
  return use_dates_[i - 1];
}

void UsageHistoryInformation::set_use_date(std::optional<base::Time> time,
                                           size_t i) {
  CHECK(1 <= i && i <= usage_history_size());
  CHECK(time.has_value() || i > 1);
  use_dates_[i - 1] = time;
}

void UsageHistoryInformation::RecordUseDate(base::Time time) {
  std::rotate(use_dates_.rbegin(), use_dates_.rbegin() + 1, use_dates_.rend());
  set_use_date(time, 1);
}

double UsageHistoryInformation::GetRankingScore(base::Time current_time) const {
  // Usually `use_count_ >= 1`, but it's not guaranteed because the value may
  // come directly from the (possibly corrupted) database. To avoid division by
  // zero, we enforce that `use_count >= 1`.
  size_t use_count = use_count_ > 0 ? use_count_ : 1;
  return -log(static_cast<double>(GetDaysSinceLastUse(current_time)) + 2) /
         log(static_cast<double>(use_count) + 1);
}

void UsageHistoryInformation::MergeUsageHistories(
    const UsageHistoryInformation& other) {
  // Update the use-count to be the max of the two merge-counts. Alternatively,
  // we could have summed the two merge-counts. We don't sum because it skews
  // the ranking score value on merge and double counts usage on profile reuse.
  // Profile reuse is accounted for on RecordUseOf() on selection of a profile
  // in the autofill drop-down; we don't need to account for that here. Further,
  // a similar, fully-typed submission that merges to an existing profile should
  // not be counted as a reuse of that profile.
  set_use_count(std::max(use_count(), other.use_count()));

  // Take the `usage_history_size()` latest use dates (nullopts go last).
  use_dates_.insert(use_dates_.end(), other.use_dates_.begin(),
                    other.use_dates_.end());
  std::ranges::sort(use_dates_, std::greater<>());
  use_dates_.resize(usage_history_size());
}

bool UsageHistoryInformation::UseDateEqualsInSeconds(
    const UsageHistoryInformation& other) const {
  return (other.use_date() - use_date()).InSeconds() == 0;
}

bool UsageHistoryInformation::HasGreaterRankingThan(
    const UsageHistoryInformation& other,
    base::Time comparison_time) const {
  double score = GetRankingScore(comparison_time);
  double other_score = other.GetRankingScore(comparison_time);
  return UsageHistoryInformation::CompareRankingScores(score, other_score,
                                                       other.use_date());
}

bool UsageHistoryInformation::CompareRankingScores(
    double lhs_score,
    double rhs_score,
    base::Time rhs_use_date) const {
  // We use rounded values to express near equivalence.
  //
  // We cannot use `std::fabs(x - y) < kEpsilon` because that'd break
  // transitivity of equivalence, which is required by std::sort().
  //
  // We don't need to worry about overflows because the maximum absolute values
  // of the scores are
  //   std::log(std::numeric_limits<double>::max()) / std::log(2)
  // which is ~1023.
  static constexpr double kEpsilon = 0.00001;
  const int32_t lhs_score_rounded = std::lround(lhs_score / kEpsilon);
  const int32_t rhs_score_rounded = std::lround(rhs_score / kEpsilon);

  if (lhs_score_rounded != rhs_score_rounded) {
    return lhs_score_rounded > rhs_score_rounded;
  }
  return use_date() > rhs_use_date;
}

}  // namespace autofill
