// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_data_model.h"

#include <math.h>

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "url/gurl.h"

namespace autofill {

AutofillDataModel::AutofillDataModel(const std::string& guid,
                                     const std::string& origin)
    : guid_(guid), origin_(origin), use_count_(1) {
  set_use_date(AutofillClock::Now());
  set_modification_date(AutofillClock::Now());
}
AutofillDataModel::~AutofillDataModel() {}

int AutofillDataModel::GetDaysSinceLastUse(base::Time current_time) const {
  if (current_time <= use_date_)
    return 0;

  return (current_time - use_date_).InDays();
}

bool AutofillDataModel::IsVerified() const {
  return !origin_.empty() && !GURL(origin_).is_valid();
}

double AutofillDataModel::GetRankingScore(base::Time current_time) const {
  return -log(static_cast<double>(GetDaysSinceLastUse(current_time)) + 2) /
         log(use_count_ + 1);
}

// TODO(crbug.com/629507): Add support for injected mock clock for testing.
void AutofillDataModel::RecordUse() {
  ++use_count_;
  set_use_date(AutofillClock::Now());
}

bool AutofillDataModel::UseDateEqualsInSeconds(
    const AutofillDataModel* other) const {
  return !((other->use_date() - use_date()).InSeconds());
}

bool AutofillDataModel::HasGreaterRankingThan(
    const AutofillDataModel* other,
    base::Time comparison_time) const {
  double score = GetRankingScore(comparison_time);
  double other_score = other->GetRankingScore(comparison_time);

  // Ties are broken by MRU, then by GUID comparison.
  const double kEpsilon = 0.00001;
  if (std::fabs(score - other_score) > kEpsilon)
    return score > other_score;

  if (use_date_ != other->use_date_)
    return use_date_ > other->use_date_;

  return guid_ > other->guid_;
}

AutofillMetadata AutofillDataModel::GetMetadata() const {
  AutofillMetadata metadata;
  metadata.use_count = use_count_;
  metadata.use_date = use_date_;
  return metadata;
}

bool AutofillDataModel::SetMetadata(const AutofillMetadata& metadata) {
  use_count_ = metadata.use_count;
  use_date_ = metadata.use_date;
  return true;
}

bool AutofillDataModel::IsDeletable() const {
  return IsAutofillEntryWithUseDateDeletable(use_date_);
}

}  // namespace autofill
