// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/device_policy_status_provider_lacros.h"

DevicePolicyStatusProviderLacros::DevicePolicyStatusProviderLacros()
    : PolicyStatusProvider() {}

DevicePolicyStatusProviderLacros::~DevicePolicyStatusProviderLacros() {}

void DevicePolicyStatusProviderLacros::SetDevicePolicyStatus(
    base::Value status) {
  device_policy_status_ = std::move(status);
}

void DevicePolicyStatusProviderLacros::GetStatus(base::DictionaryValue* dict) {
  if (!device_policy_status_.is_dict()) {
    return;
  }
  base::DictionaryValue* dict_value;
  device_policy_status_.GetAsDictionary(&dict_value);
  dict->Swap(dict_value);
}
