// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/security_settings_provider.h"

#include "base/feature_list.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/browser/web_ui_data_source.h"

namespace settings {

void AddSecurityData(content::WebUIDataSource* html_source) {
  html_source->AddBoolean("enableHttpsFirstModeNewSettings",
                          IsBalancedModeAvailable());

  html_source->AddBoolean("enableHashPrefixRealTimeLookups",
                          safe_browsing::hash_realtime_utils::
                              IsHashRealTimeLookupEligibleInSession());

  html_source->AddBoolean(
      "extendedReportingRemovePrefDependency",
      base::FeatureList::IsEnabled(
          safe_browsing::kExtendedReportingRemovePrefDependency));

  html_source->AddBoolean(
      "hashPrefixRealTimeLookupsSamplePing",
      base::FeatureList::IsEnabled(
          safe_browsing::kHashPrefixRealTimeLookupsSamplePing));
}

}  // namespace settings
