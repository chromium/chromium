// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace data_sharing::features {
namespace {
const char kDataSharingDefaultUrl[] = "https://www.google.com/chrome/tabshare/";
const char kLearnMoreSharedTabGroupPageDefaultUrl[] =
    "https://support.google.com/chrome/?p=chrome_collaboration";
const char kLearnAboutBlockedAccountsDefaultUrl[] =
    "https://support.google.com/accounts/answer/6388749";
const char kActivityLogsDefaultUrl[] =
    "https://myactivity.google.com/product/"
    "chrome_shared_tab_group_activity?utm_source=chrome_collab";

}  // namespace

BASE_FEATURE(kCollaborationAutomotive,
             "CollaborationAutomotive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCollaborationEntrepriseV2,
             "CollaborationEntrepriseV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDataSharingFeature,
             "DataSharing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDataSharingJoinOnly,
             "DataSharingJoinOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDataSharingNonProductionEnvironment,
             "DataSharingNonProductionEnvironment",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedDataTypesKillSwitch,
             "kSharedDataTypesKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDataSharingEnableUpdateChromeUI,
             "DataSharingEnableUpdateChromeUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDataSharingFunctionalityEnabled() {
  // Check if any of the primary data sharing features are enabled i.e. user is allowed to
  // create/join.
  const bool is_primary_data_sharing_enabled =
      base::FeatureList::IsEnabled(data_sharing::features::kDataSharingFeature) ||
      base::FeatureList::IsEnabled(data_sharing::features::kDataSharingJoinOnly);

  const bool is_kill_switch_enabled =
      base::FeatureList::IsEnabled(data_sharing::features::kSharedDataTypesKillSwitch);

  // If the kill switch is disabled, then 'kDataSharingEnableUpdateChromeUI'
  // must also be enabled for data sharing functionality to be enabled.
  if (!is_kill_switch_enabled) {
    return is_primary_data_sharing_enabled &&
           base::FeatureList::IsEnabled(data_sharing::features::kDataSharingEnableUpdateChromeUI);
  }

  // If the kill switch is enabled, then data sharing functionality
  // depends solely on whether a primary feature is enabled.
  return is_primary_data_sharing_enabled;
}

constexpr base::FeatureParam<std::string> kDataSharingURL(
    &kDataSharingFeature,
    "data_sharing_url",
    kDataSharingDefaultUrl);

constexpr base::FeatureParam<ServerEnvironment>::Option
    kServerEnvironmentOptions[] = {
        {ServerEnvironment::kProduction, "production"},
        {ServerEnvironment::kStaging, "staging"},
        {ServerEnvironment::kAutopush, "autopush"}};

constexpr base::FeatureParam<ServerEnvironment> kServerEnvironment(
    &kDataSharingNonProductionEnvironment,
    "server_environment",
    ServerEnvironment::kAutopush,
    &kServerEnvironmentOptions);

ServerEnvironment GetServerEnvironmentParam() {
  if (base::FeatureList::IsEnabled(kDataSharingNonProductionEnvironment)) {
    return kServerEnvironment.Get();
  } else {
    return ServerEnvironment::kProduction;
  }
}

constexpr base::FeatureParam<std::string> kLearnMoreSharedTabGroupPageURL(
    &kDataSharingFeature,
    "learn_more_shared_tab_group_page_url",
    kLearnMoreSharedTabGroupPageDefaultUrl);

constexpr base::FeatureParam<std::string> kLearnAboutBlockedAccountsURL(
    &kDataSharingFeature,
    "learn_about_blocked_accounts_url",
    kLearnAboutBlockedAccountsDefaultUrl);

constexpr base::FeatureParam<std::string> kActivityLogsURL(
    &kDataSharingFeature,
    "activity_logs_url",
    kActivityLogsDefaultUrl);

constexpr base::FeatureParam<base::TimeDelta>
    kDataSharingGroupDataPeriodicPollingInterval(
        &kDataSharingFeature,
        "data_sharing_group_data_periodic_polling_interval",
        base::Days(1));

}  // namespace data_sharing::features
