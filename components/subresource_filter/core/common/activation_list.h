// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_

#include <iosfwd>

namespace subresource_filter {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ActivationList)
enum class ActivationList : int {
  NONE = 0,
  SOCIAL_ENG_ADS_INTERSTITIAL = 1,
  PHISHING_INTERSTITIAL = 2,

  // Site violates the better ads standard.
  BETTER_ADS = 4,

  ABUSIVE = 5,

  // Make sure new elements added update the LAST value.
  LAST = ABUSIVE
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ActivationList)

// For logging use only.
std::ostream& operator<<(std::ostream& os, const ActivationList& type);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
