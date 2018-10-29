// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_model.h"

#include <math.h>

#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "url/gurl.h"

namespace autofill {

AutofillDataModel::AutofillDataModel(const std::string& guid,
                                     const std::string& origin)
    : guid_(guid),
      origin_(origin),
      use_count_(1),
      use_date_(AutofillClock::Now()),
      modification_date_(AutofillClock::Now()) {}
AutofillDataModel::~AutofillDataModel() {}

bool AutofillDataModel::IsVerified() const {
  return !origin_.empty() && !GURL(origin_).is_valid();
}

// TODO(crbug.com/629507): Add support for injected mock clock for testing.
void AutofillDataModel::RecordUse() {
  ++use_count_;
  use_date_ = AutofillClock::Now();
}

bool AutofillDataModel::CompareFrecency(const AutofillDataModel* other,
                                        base::Time comparison_time) const {
  double score = GetFrecencyScore(comparison_time);
  double other_score = other->GetFrecencyScore(comparison_time);

  // Ties are broken by MRU, then by GUID comparison.
  if (score != other_score)
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

bool AutofillDataModel::SetMetadata(const AutofillMetadata metadata) {
  use_count_ = metadata.use_count;
  use_date_ = metadata.use_date;
  return true;
}

double AutofillDataModel::GetFrecencyScore(base::Time time) const {
  // The formula calculates a score based on both the frequency and the recency
  // of the profile and leveraging the properties of the logarithmic function.
  // DaysSinceLastUse() and |use_count_| are offset because their minimum values
  // are respectively 0 and 1 but the formula requires at least a value of 2.
  // Please update getFrecencyScore in PaymentRequestImpl.java as well if below
  // formula needs update.
  return -log((time - use_date_).InDays() + 2) / log(use_count_ + 1);
}

}  // namespace autofill
