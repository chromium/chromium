// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_

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
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SETTINGS_PAGE_HELPER_H_
