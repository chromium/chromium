// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"

DeviceCloudPolicyStatusProviderChromeOS::
    DeviceCloudPolicyStatusProviderChromeOS(
        const policy::BrowserPolicyConnectorAsh* connector)
    : CloudPolicyCoreStatusProvider(
          connector->GetDeviceCloudPolicyManager()->core()) {
  enterprise_domain_manager_ = connector->GetEnterpriseDomainManager();
}

DeviceCloudPolicyStatusProviderChromeOS::
    ~DeviceCloudPolicyStatusProviderChromeOS() = default;

void DeviceCloudPolicyStatusProviderChromeOS::GetStatus(
    base::DictionaryValue* dict) {
  policy::PolicyStatusProvider::GetStatusFromCore(core_, dict);
  dict->SetStringKey("enterpriseDomainManager", enterprise_domain_manager_);
  GetOffHoursStatus(dict);
}
