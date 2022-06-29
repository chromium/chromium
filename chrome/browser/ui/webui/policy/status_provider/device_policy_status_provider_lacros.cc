// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/device_policy_status_provider_lacros.h"

#include <utility>

DevicePolicyStatusProviderLacros::DevicePolicyStatusProviderLacros()
    : PolicyStatusProvider() {}

DevicePolicyStatusProviderLacros::~DevicePolicyStatusProviderLacros() {}

void DevicePolicyStatusProviderLacros::SetDevicePolicyStatus(
    base::Value::Dict status) {
  device_policy_status_ = std::move(status);
}

void DevicePolicyStatusProviderLacros::GetStatus(base::DictionaryValue* dict) {
  static_cast<base::Value&>(*dict) =
      base::Value(std::move(device_policy_status_));
}
