// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_DEVICE_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_DEVICE_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_

#include "chrome/browser/ui/webui/policy/status_provider/cloud_policy_core_status_provider.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

// A cloud policy status provider for device policy.
class DeviceCloudPolicyStatusProviderChromeOS
    : public CloudPolicyCoreStatusProvider {
 public:
  explicit DeviceCloudPolicyStatusProviderChromeOS(
      const policy::BrowserPolicyConnectorAsh* connector);

  DeviceCloudPolicyStatusProviderChromeOS(
      const DeviceCloudPolicyStatusProviderChromeOS&) = delete;
  DeviceCloudPolicyStatusProviderChromeOS& operator=(
      const DeviceCloudPolicyStatusProviderChromeOS&) = delete;

  ~DeviceCloudPolicyStatusProviderChromeOS() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  std::string enterprise_domain_manager_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_DEVICE_CLOUD_POLICY_STATUS_PROVIDER_CHROMEOS_H_
