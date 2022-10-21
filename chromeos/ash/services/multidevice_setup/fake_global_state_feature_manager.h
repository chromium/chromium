// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_

#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"

namespace ash {

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
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_GLOBAL_STATE_FEATURE_MANAGER_H_
