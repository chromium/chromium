// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFeatureManager : public PasswordFeatureManager {
 public:
  MockPasswordFeatureManager();
  ~MockPasswordFeatureManager() override;

  MOCK_METHOD(bool, IsGenerationEnabled, (), (override, const));
  MOCK_METHOD(bool, IsAccountStorageEnabled, (), (override, const));

  MOCK_METHOD(features_util::PasswordAccountStorageUsageLevel,
              ComputePasswordAccountStorageUsageLevel,
              (),
              (override, const));
  MOCK_METHOD(bool,
              IsBiometricAuthenticationBeforeFillingEnabled,
              (),
              (override, const));

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, ShouldUpdateGmsCore, (), (override));
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
