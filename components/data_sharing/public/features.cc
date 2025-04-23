// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace data_sharing::features {
namespace {
const char kDataSharingDefaultUrl[] = "https://www.google.com/chrome/tabshare/";
const char kLearnMoreSharedTabGroupPageDefaultUrl[] = "https://support.google.com/chrome/?p=chrome_collaboration";
const char kLearnAboutBlockedAccountsDefaultUrl[] = "https://support.google.com/accounts/answer/6388749";
const char kActivityLogsDefaultUrl[] = "https://myactivity.google.com/product/chrome_shared_tab_group_activity?utm_source=chrome_collab";

}

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

bool IsDataSharingFunctionalityEnabled() {
  return base::FeatureList::IsEnabled(
             data_sharing::features::kDataSharingFeature) ||
         base::FeatureList::IsEnabled(
             data_sharing::features::kDataSharingJoinOnly);
}

BASE_FEATURE(kDataSharingNonProductionEnvironment,
             "DataSharingNonProductionEnvironment",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
