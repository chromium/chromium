// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include "components/policy/core/common/cloud/cloud_policy_util.h"

namespace device_signals {

std::string GetOsName() {
  return policy::GetOSPlatform();
}

std::string GetOsVersion() {
  return policy::GetOSVersion();
}

}  // namespace device_signals
