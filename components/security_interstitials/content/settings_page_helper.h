// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_

#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"

namespace content {
class WebContents;
}

namespace security_interstitials {

// Interface to open a settings page in a security interstitial.
class SettingsPageHelper {
 public:
  SettingsPageHelper() = default;
  virtual ~SettingsPageHelper() = default;
  SettingsPageHelper(const SettingsPageHelper&) = delete;
  SettingsPageHelper& operator=(const SettingsPageHelper&) = delete;

  // Opens the settings page that contains enhanced protection.
  virtual void OpenEnhancedProtectionSettings(
      content::WebContents* web_contents) const = 0;

  // Opens the settings page that contains enhanced protection with the
  // triggering of an in-product-help bubble over the Enhanced Safe
  // Browsing radio button.
  virtual void OpenEnhancedProtectionSettingsWithIph(
      content::WebContents* web_contents,
      safe_browsing::SafeBrowsingSettingReferralMethod referral_method)
      const = 0;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_
