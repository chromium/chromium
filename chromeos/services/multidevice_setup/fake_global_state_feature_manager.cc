// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_global_state_feature_manager.h"

namespace chromeos {

namespace multidevice_setup {

FakeGlobalStateFeatureManager::FakeGlobalStateFeatureManager()
    : GlobalStateFeatureManager() {}

FakeGlobalStateFeatureManager::~FakeGlobalStateFeatureManager() = default;

void FakeGlobalStateFeatureManager::SetIsFeatureEnabled(bool enabled) {
  is_feature_enabled_ = enabled;
}

bool FakeGlobalStateFeatureManager::IsFeatureEnabled() {
  return is_feature_enabled_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
