// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

UserCloudPolicyStatusProviderChromeOS::UserCloudPolicyStatusProviderChromeOS(
    policy::CloudPolicyCore* core,
    Profile* profile)
    : UserCloudPolicyStatusProvider(core, profile) {
  profile_ = profile;
}

UserCloudPolicyStatusProviderChromeOS::
    ~UserCloudPolicyStatusProviderChromeOS() = default;

void UserCloudPolicyStatusProviderChromeOS::GetStatus(
    base::DictionaryValue* dict) {
  if (!core_->store()->is_managed())
    return;
  UserCloudPolicyStatusProvider::GetStatus(dict);
  GetUserAffiliationStatus(dict, profile_);
  GetUserManager(dict, profile_);
}
