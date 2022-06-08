// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/user_policy_status_provider_lacros.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/proto/device_management_backend.pb.h"

UserPolicyStatusProviderLacros::UserPolicyStatusProviderLacros(
    policy::PolicyLoaderLacros* loader,
    Profile* profile)
    : profile_(profile), loader_(loader) {}

UserPolicyStatusProviderLacros::~UserPolicyStatusProviderLacros() = default;

void UserPolicyStatusProviderLacros::GetStatus(base::DictionaryValue* dict) {
  enterprise_management::PolicyData* policy = loader_->GetPolicyData();
  if (!policy)
    return;
  GetStatusFromPolicyData(policy, dict);
  ExtractDomainFromUsername(dict);
  GetUserAffiliationStatus(dict, profile_);

  // Get last fetched time from policy, since we have no refresh scheduler here.
  base::Time last_refresh_time =
      policy && policy->has_timestamp()
          ? base::Time::FromJavaTime(policy->timestamp())
          : base::Time();
  dict->SetStringKey("timeSinceLastRefresh",
                     GetTimeSinceLastActionString(last_refresh_time));

  const base::Time last_refresh_attempt_time = loader_->last_fetch_timestamp();
  dict->SetStringKey("timeSinceLastFetchAttempt",
                     GetTimeSinceLastActionString(last_refresh_attempt_time));

  // TODO(https://crbug.com/1243869): Pass this information from Ash through
  // Mojo. Assume no error for now.
  dict->SetBoolKey("error", false);
  dict->SetStringKey(
      "status", FormatStoreStatus(
                    policy::CloudPolicyStore::STATUS_OK,
                    policy::CloudPolicyValidatorBase::Status::VALIDATION_OK));
}
