// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_

#include <iosfwd>

namespace subresource_filter {

// This enum backs a histogram. Make sure all updates are reflected in
// enums.xml.
enum class ActivationList : int {
  NONE,
  SOCIAL_ENG_ADS_INTERSTITIAL,
  PHISHING_INTERSTITIAL,
  SUBRESOURCE_FILTER,

  // Site violates the better ads standard.
  BETTER_ADS,

  ABUSIVE,

  // Make sure new elements added update the LAST value.
  LAST = ABUSIVE
};

// For logging use only.
std::ostream& operator<<(std::ostream& os, const ActivationList& type);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
