// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/ash/user_permission_service_ash.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/device_signals/core/browser/mock_user_delegate.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using policy::EnterpriseManagementAuthority;
using policy::ScopedManagementServiceOverrideForTesting;
using testing::Return;

namespace device_signals {

namespace {

class TestManagementService : public policy::ManagementService {
 public:
  TestManagementService() : ManagementService({}) {}
  explicit TestManagementService(
      std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers)
      : ManagementService(std::move(providers)) {}
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<policy::ManagementStatusProvider>>
          providers) {
    SetManagementStatusProvider(std::move(providers));
  }
};

}  // namespace

class UserPermissionServiceAshTest : public testing::Test {
 protected:
  explicit UserPermissionServiceAshTest(
      bool feature_unmanaged_device_enabled = true)
      : feature_unmanaged_device_enabled_(feature_unmanaged_device_enabled) {
    RegisterProfilePrefs(test_prefs_.registry());

    auto mock_user_delegate =
        std::make_unique<testing::StrictMock<MockUserDelegate>>();
    mock_user_delegate_ = mock_user_delegate.get();

    permission_service_ = std::make_unique<UserPermissionServiceAsh>(
        &management_service_, std::move(mock_user_delegate), &test_prefs_);
  }

  void SetUp() override {
    feature_list_.InitWithFeatureState(
        ash::features::kUnmanagedDeviceDeviceTrustConnectorEnabled,
        feature_unmanaged_device_enabled_);
  }

  void SetDeviceAsCloudManaged(bool is_managed = true) {
    if (is_managed) {
      scoped_override_.emplace(&management_service_,
                               EnterpriseManagementAuthority::CLOUD_DOMAIN);
    }

    EXPECT_EQ(management_service_.HasManagementAuthority(
                  EnterpriseManagementAuthority::CLOUD_DOMAIN),
              is_managed);
  }

  void SetUserAsCloudManaged(bool is_managed = true) {
    if (is_managed) {
      scoped_override_.emplace(&management_service_,
                               EnterpriseManagementAuthority::CLOUD);
    }

    EXPECT_CALL(*mock_user_delegate_, IsManagedUser())
        .WillRepeatedly(Return(is_managed));
  }

  void SetSigninContext(bool is_signin_context = true) {
    EXPECT_CALL(*mock_user_delegate_, IsSigninContext())
        .WillOnce(Return(is_signin_context));
  }

  void SetUserAffiliated(bool is_affiliated = true) {
    EXPECT_CALL(*mock_user_delegate_, IsAffiliated())
        .WillOnce(Return(is_affiliated));
  }

  base::test::TaskEnvironment task_environment_;

  TestManagementService management_service_;
  std::optional<ScopedManagementServiceOverrideForTesting> scoped_override_;
  raw_ptr<testing::StrictMock<MockUserDelegate>, DanglingUntriaged>
      mock_user_delegate_;
  TestingPrefServiceSimple test_prefs_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<UserPermissionServiceAsh> permission_service_;

  const bool feature_unmanaged_device_enabled_;
};

// Tests that ShouldCollectConsent should always return false on CrOS, as the
// consent flow is not supported yet.
TEST_F(UserPermissionServiceAshTest, ShouldCollectConsent_False) {
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}

// Tests that signals can be collected for affiliated users.
TEST_F(UserPermissionServiceAshTest,
       CanCollectSignals_DeviceCloudManaged_AffiliatedUser) {
  SetDeviceAsCloudManaged();
  SetSigninContext(/*is_signin_context=*/false);
  SetUserAffiliated();

  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

// Tests that signals can be collected on the signin screen of a managed device.
TEST_F(UserPermissionServiceAshTest,
       CanCollectSignals_DeviceCloudManaged_SigninContext) {
  SetDeviceAsCloudManaged();
  SetSigninContext();

  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

// Tests that signals cannot be collected if the device is unmanaged but user is
// deemed as affiliated. The latter can happen since we treat the login profile
// as affiliated.
TEST_F(UserPermissionServiceAshTest,
       CanCollectSignals_DeviceCloudManaged_UnaffiliatedUser) {
  SetDeviceAsCloudManaged();
  SetSigninContext(/*is_signin_context=*/false);
  SetUserAffiliated(/*is_affiliated=*/false);

  EXPECT_EQ(permission_service_->CanCollectSignals(),
            UserPermission::kUnsupported);
}

// Tests that signals cannot be collected on the signin screen if the device is
// unmanaged.
TEST_F(UserPermissionServiceAshTest,
       CanCollectSignals_UnmanagedDevice_UnmanagedUser) {
  SetDeviceAsCloudManaged(/*is_managed=*/false);
  SetUserAsCloudManaged(/*is_managed=*/false);

  EXPECT_EQ(permission_service_->CanCollectSignals(),
            UserPermission::kUnsupported);
}

// Tests that signals can be collected if the device is unmanaged and the user
// is managed.
TEST_F(UserPermissionServiceAshTest,
       CanCollectSignals_UnmanagedDevice_ManagedUser) {
  SetDeviceAsCloudManaged(/*is_managed=*/false);
  SetUserAsCloudManaged();

  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

class UserPermissionServiceAshFlagTest
    : public UserPermissionServiceAshTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  UserPermissionServiceAshFlagTest()
      : UserPermissionServiceAshTest(GetParam()) {}
};

TEST_P(UserPermissionServiceAshFlagTest, CanCollectSignals_UnmanagedDevice) {
  SetDeviceAsCloudManaged(/*is_managed=*/false);
  SetUserAsCloudManaged();

  EXPECT_EQ(
      permission_service_->CanCollectSignals(),
      GetParam() ? UserPermission::kGranted : UserPermission::kUnsupported);
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserPermissionServiceAshFlagTest,
                         testing::Bool());

}  // namespace device_signals
