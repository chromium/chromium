// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/security_settings_provider.h"

#include "base/feature_list.h"
#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_ui_data_source.h"

using safe_browsing::SecuritySettingsBundleSetting;

namespace settings {

void AddSecurityData(content::WebUIDataSource* html_source) {
  html_source->AddBoolean(
      "enableBundledSecuritySettings",
      base::FeatureList::IsEnabled(safe_browsing::kBundledSecuritySettings));
  html_source->AddBoolean("enableHttpsFirstModeNewSettings",
                          IsBalancedModeAvailable());

  html_source->AddInteger(
      "securityStandardBundleSafeBrowsingDefault",
      static_cast<int>(safe_browsing::GeneratedSafeBrowsingPref::GetDefault(
          SecuritySettingsBundleSetting::STANDARD)));
  html_source->AddInteger(
      "securityEnhancedBundleSafeBrowsingDefault",
      static_cast<int>(safe_browsing::GeneratedSafeBrowsingPref::GetDefault(
          SecuritySettingsBundleSetting::ENHANCED)));

  // TODO(http://crbug.com/458521865) Move remainder of
  // security-related-settings (not the strings) to this function.
}

}  // namespace settings
