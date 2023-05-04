// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/browser/mock_user_delegate.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using policy::EnterpriseManagementAuthority;
using policy::ScopedManagementServiceOverrideForTesting;
using testing::_;
using testing::Return;

namespace device_signals {

namespace {

constexpr char kUserGaiaId[] = "some-gaia-id";

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

class UserPermissionServiceImplTest : public testing::Test {
 protected:
  UserPermissionServiceImplTest()
      : scoped_override_(&management_service_,
                         EnterpriseManagementAuthority::CLOUD_DOMAIN) {
    RegisterProfilePrefs(test_prefs_.registry());

    auto mock_user_delegate =
        std::make_unique<testing::StrictMock<MockUserDelegate>>();
    mock_user_delegate_ = mock_user_delegate.get();

    permission_service_ = std::make_unique<UserPermissionServiceImpl>(
        &management_service_, std::move(mock_user_delegate), &test_prefs_);
  }

  base::test::TaskEnvironment task_environment_;

  TestManagementService management_service_;
  ScopedManagementServiceOverrideForTesting scoped_override_;
  raw_ptr<testing::StrictMock<MockUserDelegate>> mock_user_delegate_;
  TestingPrefServiceSimple test_prefs_;

  std::unique_ptr<UserPermissionServiceImpl> permission_service_;
};

// Tests CanUserCollectSignals with a missing user ID.
TEST_F(UserPermissionServiceImplTest, CanUserCollectSignals_EmptyUserId) {
  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kMissingUser);
}

// Tests CanUserCollectSignals with a user ID that does not represent the
// current browser user.
TEST_F(UserPermissionServiceImplTest,
       CanUserCollectSignals_UserId_NotSameUser) {
  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  // Mock that it is not the same user.
  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(false));

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kUnknownUser);
}

// Tests CanUserCollectSignals with a user ID that represents the browser user,
// but that user is not managed.
TEST_F(UserPermissionServiceImplTest, CanUserCollectSignals_User_NotManaged) {
  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManaged()).WillOnce(Return(false));

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kConsumerUser);
}

// Tests CanUserCollectSignals with a managed user ID but the browser is not
// managed and the user has not given consent.
TEST_F(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserNotManaged_NoConsent) {
  // Set management to something other than CLOUD_DOMAIN.
  ScopedManagementServiceOverrideForTesting another_scope(
      &management_service_, EnterpriseManagementAuthority::CLOUD);

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManaged()).WillOnce(Return(true));

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kMissingConsent);
}

// Tests CanUserCollectSignals with a managed user ID but the browser is not
// managed and the user has given consent.
TEST_F(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserNotManaged_WithConsent) {
  // Set management to something other than CLOUD_DOMAIN.
  ScopedManagementServiceOverrideForTesting another_scope(
      &management_service_, EnterpriseManagementAuthority::CLOUD);

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManaged()).WillOnce(Return(true));

  // Fake as if user has given consent.
  test_prefs_.SetBoolean(prefs::kDeviceSignalsConsentReceived, true);

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kGranted);
}

// Tests CanUserCollectSignals with a managed user ID and the browser is
// managed, where the user is the same as the profile user but it is not
// affiliated with the browser's org.
TEST_F(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserManaged_ProfileUser_Unaffiliated) {
  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManaged()).WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(false));

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kUnaffiliated);
}

// Tests CanUserCollectSignals with a managed user ID and the browser is
// managed, where the user is the same as the profile user and it is affiliated
// with the browser's org.
TEST_F(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserManaged_ProfileUser_Affiliated) {
  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManaged()).WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(true));

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanUserCollectSignals(user_context,
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kGranted);
}

// Tests that consent is required before allowing to collect signals from an
// unmanaged browser.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_BrowserNotManaged) {
  // Set management to something other than CLOUD_DOMAIN.
  ScopedManagementServiceOverrideForTesting another_scope(
      &management_service_, EnterpriseManagementAuthority::CLOUD);

  base::test::TestFuture<UserPermission> future;
  permission_service_->CanCollectSignals(future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kMissingConsent);
}

// Tests that signals can be collected from a managed browser.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_BrowserManaged) {
  base::test::TestFuture<UserPermission> future;
  permission_service_->CanCollectSignals(future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kGranted);
}

}  // namespace device_signals
