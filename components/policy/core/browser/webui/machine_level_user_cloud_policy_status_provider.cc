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

void MachineLevelUserCloudPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  CloudPolicyRefreshScheduler* refresh_scheduler = core_->refresh_scheduler();

  dict->SetString(
      "refreshInterval",
      ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_SHORT,
          base::Milliseconds(
              refresh_scheduler
                  ? refresh_scheduler->GetActualRefreshDelay()
                  : CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs)));
  dict->SetBoolean(
      "policiesPushAvailable",
      refresh_scheduler ? refresh_scheduler->invalidations_available() : false);

  if (!context_->enrollmentToken.empty())
    dict->SetString("enrollmentToken", context_->enrollmentToken);

  if (!context_->deviceId.empty())
    dict->SetString("deviceId", context_->deviceId);

  CloudPolicyStore* store = core_->store();
  if (store) {
    std::u16string status = GetPolicyStatusFromStore(store, core_->client());

    dict->SetString("status", status);

    const enterprise_management::PolicyData* policy = store->policy();
    if (policy) {
      dict->SetString("timeSinceLastRefresh",
                      GetTimeSinceLastRefreshString(
                          refresh_scheduler ? refresh_scheduler->last_refresh()
                                            : base::Time()));
      dict->SetString("domain", gaia::ExtractDomainName(policy->username()));
    }
  }
  dict->SetString("machine", GetMachineName());

  if (!context_->lastCloudReportSent.is_null()) {
    dict->SetString("lastCloudReportSentTimestamp",
                    base::TimeFormatShortDateAndTimeWithTimeZone(
                        context_->lastCloudReportSent));
    dict->SetString(
        "timeSinceLastCloudReportSent",
        GetTimeSinceLastRefreshString(context_->lastCloudReportSent));
  }
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
