// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/device_local_account_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

DeviceLocalAccountPolicyStatusProvider::DeviceLocalAccountPolicyStatusProvider(
    const std::string& user_id,
    policy::DeviceLocalAccountPolicyService* service)
    : user_id_(user_id), service_(service) {
  service_->AddObserver(this);
}

DeviceLocalAccountPolicyStatusProvider::
    ~DeviceLocalAccountPolicyStatusProvider() {
  service_->RemoveObserver(this);
}

void DeviceLocalAccountPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  const policy::DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(user_id_);
  if (broker) {
    policy::PolicyStatusProvider::GetStatusFromCore(broker->core(), dict);
  } else {
    dict->SetBoolKey("error", true);
    dict->SetStringKey("status",
                       policy::FormatStoreStatus(
                           policy::CloudPolicyStore::STATUS_BAD_STATE,
                           policy::CloudPolicyValidatorBase::VALIDATION_OK));
    dict->SetStringKey("username", std::string());
  }
  ExtractDomainFromUsername(dict);
  dict->SetBoolKey("publicAccount", true);
}

void DeviceLocalAccountPolicyStatusProvider::OnPolicyUpdated(
    const std::string& user_id) {
  if (user_id == user_id_)
    NotifyStatusChange();
}

void DeviceLocalAccountPolicyStatusProvider::OnDeviceLocalAccountsChanged() {
  NotifyStatusChange();
}
