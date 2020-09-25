// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_

#include "components/password_manager/core/browser/password_feature_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFeatureManager : public PasswordFeatureManager {
 public:
  MockPasswordFeatureManager();
  ~MockPasswordFeatureManager() override;

  MOCK_CONST_METHOD0(IsGenerationEnabled, bool());

  MOCK_CONST_METHOD0(IsOptedInForAccountStorage, bool());
  MOCK_CONST_METHOD0(ShouldShowAccountStorageOptIn, bool());
  MOCK_CONST_METHOD1(ShouldShowAccountStorageReSignin, bool(const GURL&));
  MOCK_METHOD0(OptInToAccountStorage, void());
  MOCK_METHOD0(OptOutOfAccountStorageAndClearSettings, void());

  MOCK_CONST_METHOD0(ShouldShowAccountStorageBubbleUi, bool());

  MOCK_METHOD1(SetDefaultPasswordStore, void(const PasswordForm::Store& store));
  MOCK_CONST_METHOD0(GetDefaultPasswordStore, PasswordForm::Store());

  MOCK_CONST_METHOD0(ComputePasswordAccountStorageUsageLevel,
                     metrics_util::PasswordAccountStorageUsageLevel());

  MOCK_METHOD0(RecordMoveOfferedToNonOptedInUser, void());
  MOCK_CONST_METHOD0(GetMoveOfferedToNonOptedInUserCount, int());
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
