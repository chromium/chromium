// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_

#include "chromeos/services/multidevice_setup/global_state_feature_manager.h"

namespace chromeos {

namespace multidevice_setup {

// Test GlobalStateFeatureManager implementation.
class FakeGlobalStateFeatureManager : public GlobalStateFeatureManager {
 public:
  FakeGlobalStateFeatureManager();
  ~FakeGlobalStateFeatureManager() override;

  // GlobalStateFeatureManager:
  void SetIsFeatureEnabled(bool enabled) override;
  bool IsFeatureEnabled() override;

 private:
  bool is_feature_enabled_ = false;
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_
