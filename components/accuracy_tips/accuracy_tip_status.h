// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_

namespace accuracy_tips {

// Represents the different results of the accuracy check.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccuracyTipStatus {
  // No accuracy information for the site.
  kNone = 0,
  // Site is eligible for showing an accuracy tip.
  kShowAccuracyTip = 1,
  // The user recently saw an accuracy tip. Accuracy tip elegibility was not
  // checked.
  kRateLimited = 2,
  // The user disabled accuracy tips. Accuracy tip elegibility was not checked.
  kOptOut = 3,
  // The site is eligible for showing an accuracy tip but the tip wasn't shown
  // as the site previously had high engagement from the user.
  kHighEnagagement = 4,
  // The site is eligible for showing an accuracy tip but the tip wasn't shown
  // as the site security state wasn't secure.
  kNotSecure = 5,

  kMaxValue = kNotSecure,
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_
