// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_

#include "chrome/browser/ui/webui/policy/status_provider/user_cloud_policy_status_provider.h"

class Profile;

namespace base {
class DictionaryValue;
}  // namespace base

namespace policy {
class CloudPolicyCore;
}  // namespace policy

// A cloud policy status provider for user policy on Chrome OS.
class UserCloudPolicyStatusProviderChromeOS
    : public UserCloudPolicyStatusProvider {
 public:
  explicit UserCloudPolicyStatusProviderChromeOS(policy::CloudPolicyCore* core,
                                                 Profile* profile);

  UserCloudPolicyStatusProviderChromeOS(
      const UserCloudPolicyStatusProviderChromeOS&) = delete;
  UserCloudPolicyStatusProviderChromeOS& operator=(
      const UserCloudPolicyStatusProviderChromeOS&) = delete;

  ~UserCloudPolicyStatusProviderChromeOS() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_
