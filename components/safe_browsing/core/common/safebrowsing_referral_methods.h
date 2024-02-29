// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_REFERRAL_METHODS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_REFERRAL_METHODS_H_

namespace safe_browsing {

// This represents the different ways a user can be referred to the security
// settings page through a promotion.
enum class SafeBrowsingSettingReferralMethod {
  kSecurityInterstitial = 0,
  kSafetyCheck = 1,
  kPromoSlingerReferral = 2,
  kDownloadBubbleSubpage = 3,
  kDownloadButtonIphPromo = 4,
  kDownloadPageRowPromo = 5,
  kMaxValue = kDownloadPageRowPromo,
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_REFERRAL_METHODS_H_
