// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/time_format.h"

namespace policy {

MachineLevelUserCloudPolicyStatusProvider::
    MachineLevelUserCloudPolicyStatusProvider(
        CloudPolicyCore* core,
        MachineLevelUserCloudPolicyContext* context)
    : core_(core), context_(context) {
  if (core_->store())
    core_->store()->AddObserver(this);
}

MachineLevelUserCloudPolicyStatusProvider::
    ~MachineLevelUserCloudPolicyStatusProvider() {
  if (core_->store())
    core_->store()->RemoveObserver(this);
}

base::Value::Dict MachineLevelUserCloudPolicyStatusProvider::GetStatus() {
  CloudPolicyRefreshScheduler* refresh_scheduler = core_->refresh_scheduler();

  base::Value::Dict dict;
  dict.Set("refreshInterval",
           ui::TimeFormat::Simple(
               ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_SHORT,
               base::Milliseconds(
                   refresh_scheduler
                       ? refresh_scheduler->GetActualRefreshDelay()
                       : CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs)));
  dict.Set(
      "policiesPushAvailable",
      refresh_scheduler ? refresh_scheduler->invalidations_available() : false);

  if (!context_->enrollmentToken.empty())
    dict.Set("enrollmentToken", context_->enrollmentToken);

  if (!context_->deviceId.empty())
    dict.Set("deviceId", context_->deviceId);

  CloudPolicyStore* store = core_->store();
  if (store) {
    std::u16string status = GetPolicyStatusFromStore(store, core_->client());

    dict.Set("status", status);

    const enterprise_management::PolicyData* policy = store->policy();
    if (policy) {
      dict.Set("timeSinceLastRefresh",
               GetTimeSinceLastActionString(
                   refresh_scheduler ? refresh_scheduler->last_refresh()
                                     : base::Time()));
      dict.Set("domain", gaia::ExtractDomainName(policy->username()));
    }
  }
  dict.Set("machine", GetMachineName());

  if (!context_->lastCloudReportSent.is_null()) {
    dict.Set("lastCloudReportSentTimestamp",
             base::TimeFormatShortDateAndTimeWithTimeZone(
                 context_->lastCloudReportSent));
    dict.Set("timeSinceLastCloudReportSent",
             GetTimeSinceLastActionString(context_->lastCloudReportSent));
  }
  return dict;
}

void MachineLevelUserCloudPolicyStatusProvider::OnStoreLoaded(
    CloudPolicyStore* store) {
  NotifyStatusChange();
}

void MachineLevelUserCloudPolicyStatusProvider::OnStoreError(
    CloudPolicyStore* store) {
  NotifyStatusChange();
}

}  // namespace policy
