// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/security_settings_provider.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
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
  html_source->AddBoolean(
      "enableBundledSecuritySettingsSecureDnsV2",
      base::FeatureList::IsEnabled(
          safe_browsing::kBundledSecuritySettingsSecureDnsV2));
  html_source->AddBoolean("enableHttpsFirstModeNewSettings",
                          IsBalancedModeAvailable());

  html_source->AddInteger("securityStandardBundleSafeBrowsingDefault",
                          static_cast<int>(GetDefaultSafeBrowsingState(
                              SecuritySettingsBundleSetting::STANDARD)));
  html_source->AddInteger("securityEnhancedBundleSafeBrowsingDefault",
                          static_cast<int>(GetDefaultSafeBrowsingState(
                              SecuritySettingsBundleSetting::ENHANCED)));

  html_source->AddInteger(
      "securityStandardBundleJavascriptGuardrailsDefault",
      static_cast<int>(content_settings::GeneratedJavascriptOptimizerPref::
                           GetDefaultJsOptimizerSetting(
                               SecuritySettingsBundleSetting::STANDARD)));
  html_source->AddInteger(
      "securityEnhancedBundleJavascriptGuardrailsDefault",
      static_cast<int>(content_settings::GeneratedJavascriptOptimizerPref::
                           GetDefaultJsOptimizerSetting(
                               SecuritySettingsBundleSetting::ENHANCED)));

  // TODO(http://crbug.com/458521865) Move remainder of
  // security-related-settings (not the strings) to this function.
}

}  // namespace settings
