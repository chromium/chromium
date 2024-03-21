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
  MOCK_METHOD(bool, IsOptedInForAccountStorage, (), (override, const));
  MOCK_METHOD(bool, ShouldShowAccountStorageOptIn, (), (override, const));
  MOCK_METHOD(bool,
              ShouldShowAccountStorageReSignin,
              (const GURL&),
              (override, const));
  MOCK_METHOD(bool, ShouldShowAccountStorageBubbleUi, (), (override, const));
  MOCK_METHOD(PasswordForm::Store,
              GetDefaultPasswordStore,
              (),
              (override, const));
  MOCK_METHOD(bool, IsDefaultPasswordStoreSet, (), (override, const));

  MOCK_METHOD(features_util::PasswordAccountStorageUsageLevel,
              ComputePasswordAccountStorageUsageLevel,
              (),
              (override, const));
  MOCK_METHOD(bool,
              IsBiometricAuthenticationBeforeFillingEnabled,
              (),
              (override, const));

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, OptInToAccountStorage, (), (override));
  MOCK_METHOD(void, OptOutOfAccountStorage, (), (override));
  MOCK_METHOD(void, OptOutOfAccountStorageAndClearSettings, (), (override));
  MOCK_METHOD(bool,
              ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally,
              (),
              (override, const));
  MOCK_METHOD(void,
              SetDefaultPasswordStore,
              (const PasswordForm::Store& store),
              (override));
  MOCK_METHOD(bool, ShouldChangeDefaultPasswordStore, (), (override, const));
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, ShouldUpdateGmsCore, (), (override));
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
