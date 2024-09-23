// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/device_signals/core/browser/mock_user_delegate.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/device_signals/core/common/signals_features.h"
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
using testing::AnyNumber;
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

class UserPermissionServiceImplTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 protected:
  UserPermissionServiceImplTest() {
    RegisterProfilePrefs(test_prefs_.registry());
    scoped_feature_list_.InitWithFeatureState(
        enterprise_signals::features::kNewEvSignalsUnaffiliatedEnabled,
        is_new_ev_signals_unaffiliated_enabled());

    auto mock_user_delegate =
        std::make_unique<testing::StrictMock<MockUserDelegate>>();
    mock_user_delegate_ = mock_user_delegate.get();

    permission_service_ = std::make_unique<UserPermissionServiceImpl>(
        &management_service_, std::move(mock_user_delegate), &test_prefs_);
  }
  ~UserPermissionServiceImplTest() override { mock_user_delegate_ = nullptr; }

  void SetDeviceAsCloudManaged() {
    scoped_override_.emplace(&management_service_,
                             EnterpriseManagementAuthority::CLOUD_DOMAIN);
  }

  void SetUserAsCloudManaged() {
    scoped_override_.emplace(&management_service_,
                             EnterpriseManagementAuthority::CLOUD);
  }

  void SetUserConsentGiven() {
    // Fake as if user has given consent.
    test_prefs_.SetBoolean(prefs::kDeviceSignalsConsentReceived, true);
  }

  void SetPermanentUserConsentGiven() {
    // Fake as if user has given permanent consent.
    test_prefs_.SetBoolean(prefs::kDeviceSignalsPermanentConsentReceived, true);
  }

  void SetPolicyScopesNeedingSignals(bool machine_scope, bool user_scope) {
    std::set<policy::PolicyScope> scopes;
    if (machine_scope) {
      scopes.insert(policy::POLICY_SCOPE_MACHINE);
    }
    if (user_scope) {
      scopes.insert(policy::POLICY_SCOPE_USER);
    }
    EXPECT_CALL(*mock_user_delegate_, GetPolicyScopesNeedingSignals())
        .WillOnce(Return(std::move(scopes)));
  }

  void EnableConsentFlowPolicy() { SetConsentFlowPolicy(true); }

  void SetConsentFlowPolicy(bool value) {
    test_prefs_.SetBoolean(prefs::kUnmanagedDeviceSignalsConsentFlowEnabled,
                           value);
  }

  bool is_new_ev_signals_unaffiliated_enabled() { return GetParam(); }

  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_;
  TestManagementService management_service_;
  std::optional<ScopedManagementServiceOverrideForTesting> scoped_override_;
  raw_ptr<testing::StrictMock<MockUserDelegate>> mock_user_delegate_;
  TestingPrefServiceSimple test_prefs_;

  std::unique_ptr<UserPermissionServiceImpl> permission_service_;
};

// Tests that consent does not need to be collected if it was already given.
TEST_P(UserPermissionServiceImplTest, ShouldCollectConsent_ConsentGiven) {
  SetUserConsentGiven();
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}
TEST_P(UserPermissionServiceImplTest,
       ShouldCollectConsent_PermanentConsentGiven) {
  SetPermanentUserConsentGiven();
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}

struct DeviceManagedSpecificPolicyTestCase {
  bool policy_enabled = false;
  bool managed_user = false;
  bool is_affiliated = false;
  bool should_collect_consent = false;
};

// Tests that consent does not need to be collected if the device is cloud
// managed.
TEST_P(UserPermissionServiceImplTest, ShouldCollectConsent_DeviceCloudManaged) {
  SetDeviceAsCloudManaged();

  std::array<DeviceManagedSpecificPolicyTestCase, 6> test_cases = {
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/true, /*managed_user=*/true,
          /*is_affiliated=*/true, /*should_collect_consent=*/false},
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/true, /*managed_user=*/true,
          /*is_affiliated=*/false,
          /*should_collect_consent=*/is_new_ev_signals_unaffiliated_enabled()},
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/true, /*managed_user=*/false,
          /*is_affiliated=*/false, /*should_collect_consent=*/false},
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/false, /*managed_user=*/true,
          /*is_affiliated=*/true, /*should_collect_consent=*/false},
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/false, /*managed_user=*/true,
          /*is_affiliated=*/false, /*should_collect_consent=*/false},
      DeviceManagedSpecificPolicyTestCase{
          /*policy_enabled=*/false, /*managed_user=*/false,
          /*is_affiliated=*/false, /*should_collect_consent=*/false},
  };

  for (const auto& test_case : test_cases) {
    SetPolicyScopesNeedingSignals(/*machine_scope=*/false,
                                  /*user_scope*/ false);
    SetConsentFlowPolicy(test_case.policy_enabled);

    EXPECT_CALL(*mock_user_delegate_, IsManagedUser())
        .Times(AnyNumber())
        .WillOnce(Return(test_case.managed_user));
    EXPECT_CALL(*mock_user_delegate_, IsAffiliated())
        .Times(AnyNumber())
        .WillOnce(Return(test_case.is_affiliated));

    EXPECT_EQ(permission_service_->ShouldCollectConsent(),
              test_case.should_collect_consent);
  }
}

// Tests that consent does not need to be collected if the device is not cloud
// managed but the "enable consent flow" policy is not enabled.
TEST_P(UserPermissionServiceImplTest,
       ShouldCollectConsent_NoEnableConsentFlowPolicy) {
  SetUserAsCloudManaged();
  SetPolicyScopesNeedingSignals(/*machine_scope=*/false, /*user_scope*/ false);
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}

// Tests that consent needs to be collected if the device is not cloud managed
// and the "enable consent flow" policy is enabled.
TEST_P(UserPermissionServiceImplTest,
       ShouldCollectConsent_SpecificPolicy_ManagedUser) {
  SetUserAsCloudManaged();
  EnableConsentFlowPolicy();
  SetPolicyScopesNeedingSignals(/*machine_scope=*/false, /*user_scope*/ false);
  EXPECT_TRUE(permission_service_->ShouldCollectConsent());
}

struct DeviceManagedDependentPolicyTestCase {
  bool machine_scope = false;
  bool user_scope = false;
  bool managed_user = false;
  bool is_affiliated = false;
  bool should_collect_consent = false;
};

// Tests the behavior of ShouldCollectConsent against all permutations of
// dependent policy scope and affiliation.
TEST_P(UserPermissionServiceImplTest,
       ShouldCollectConsent_ManagedDevice_DependentPolicy) {
  SetDeviceAsCloudManaged();

  // Testing mostly all permutations of possible scenarios. The only special
  // property here is `managed_user`, which only make sense to be tested as
  // false when both `user_scope` and `is_affiliated` are also false.
  std::array<DeviceManagedDependentPolicyTestCase, 10> test_cases = {
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/true, /*user_scope=*/true, /*managed_user=*/true,
          /*is_affiliated=*/true,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/true, /*user_scope=*/true, /*managed_user=*/true,
          /*is_affiliated=*/false,
          /*should_collect_consent=*/true},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/true, /*user_scope=*/false, /*managed_user=*/true,
          /*is_affiliated=*/true,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/true, /*user_scope=*/false, /*managed_user=*/true,
          /*is_affiliated=*/false,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/true, /*user_scope=*/false, /*managed_user=*/false,
          /*is_affiliated=*/false,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/false, /*user_scope=*/true, /*managed_user=*/true,
          /*is_affiliated=*/true,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/false, /*user_scope=*/true, /*managed_user=*/true,
          /*is_affiliated=*/false,
          /*should_collect_consent=*/true},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/false, /*user_scope=*/false, /*managed_user=*/true,
          /*is_affiliated=*/true,
          /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/false, /*user_scope=*/false, /*managed_user=*/true,
          /*is_affiliated=*/false, /*should_collect_consent=*/false},
      DeviceManagedDependentPolicyTestCase{
          /*machine_scope=*/false, /*user_scope=*/false, /*managed_user=*/false,
          /*is_affiliated=*/false, /*should_collect_consent=*/false},
  };

  for (const auto& test_case : test_cases) {
    SetPolicyScopesNeedingSignals(test_case.machine_scope,
                                  test_case.user_scope);

    EXPECT_CALL(*mock_user_delegate_, IsManagedUser())
        .Times(AnyNumber())
        .WillOnce(Return(test_case.managed_user));
    EXPECT_CALL(*mock_user_delegate_, IsAffiliated())
        .Times(AnyNumber())
        .WillOnce(Return(test_case.is_affiliated));

    EXPECT_EQ(permission_service_->ShouldCollectConsent(),
              test_case.should_collect_consent);
  }
}

// Tests that consent should be collected when a dependent policy is enabled on
// an unmanaged device.
TEST_P(UserPermissionServiceImplTest,
       ShouldCollectConsent_UnmanagedDevice_DependentPolicy) {
  SetUserAsCloudManaged();
  SetPolicyScopesNeedingSignals(/*machine_scope=*/false, /*user_scope*/ true);
  EXPECT_TRUE(permission_service_->ShouldCollectConsent());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace {
constexpr char kUserGaiaId[] = "some-gaia-id";
}  // namespace

// Tests CanUserCollectSignals with a missing user ID.
TEST_P(UserPermissionServiceImplTest, CanUserCollectSignals_EmptyUserId) {
  SetDeviceAsCloudManaged();

  UserContext user_context;
  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kMissingUser);
}

// Tests CanUserCollectSignals with a user ID that does not represent the
// current browser user.
TEST_P(UserPermissionServiceImplTest,
       CanUserCollectSignals_UserId_NotSameUser) {
  SetDeviceAsCloudManaged();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  // Mock that it is not the same user.
  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(false));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kUnknownUser);
}

// Tests CanUserCollectSignals with a user ID that represents the browser user,
// but that user is not managed.
TEST_P(UserPermissionServiceImplTest, CanUserCollectSignals_User_NotManaged) {
  SetDeviceAsCloudManaged();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(false));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kConsumerUser);
}

// Tests CanUserCollectSignals with a managed user ID but the browser is not
// managed and the user has not given consent.
TEST_P(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserNotManaged_NoConsent) {
  SetUserAsCloudManaged();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kMissingConsent);
}

// Tests CanUserCollectSignals with a managed user ID but the browser is not
// managed and the user has given consent.
TEST_P(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserNotManaged_WithConsent) {
  SetUserAsCloudManaged();
  SetUserConsentGiven();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kGranted);
}

// Tests CanUserCollectSignals with a managed user ID and the browser is
// managed, where the user is the same as the profile user but it is not
// affiliated with the browser's org.
TEST_P(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserManaged_ProfileUser_Unaffiliated) {
  SetDeviceAsCloudManaged();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(false));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kMissingConsent);
}

// Tests CanUserCollectSignals with a managed user ID and the browser is
// managed, where the user is the same as the profile user and it is affiliated
// with the browser's org.
TEST_P(UserPermissionServiceImplTest,
       CanUserCollectSignals_BrowserManaged_ProfileUser_Affiliated) {
  SetDeviceAsCloudManaged();

  UserContext user_context;
  user_context.user_id = kUserGaiaId;

  EXPECT_CALL(*mock_user_delegate_, IsSameUser(kUserGaiaId))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(true));

  EXPECT_EQ(permission_service_->CanUserCollectSignals(user_context),
            UserPermission::kGranted);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX

// Tests that signals can be collected if the user has already given their
// consent.
TEST_P(UserPermissionServiceImplTest, CanCollectSignals_AlreadyConsented) {
  SetUserConsentGiven();
  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}
TEST_P(UserPermissionServiceImplTest, CanCollectSignals_PermanentConsent) {
  SetPermanentUserConsentGiven();
  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

// Tests that consent is required before allowing to collect signals from an
// unmanaged browser.
TEST_P(UserPermissionServiceImplTest, CanCollectSignals_BrowserNotManaged) {
  SetUserAsCloudManaged();
  EXPECT_EQ(permission_service_->CanCollectSignals(),
            UserPermission::kMissingConsent);
}

// Tests that signals can be collected when on a managed browser in an unmanaged
// profile.
TEST_P(UserPermissionServiceImplTest,
       CanCollectSignals_BrowserManaged_UnmanagedUser) {
  SetDeviceAsCloudManaged();

  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(false));

  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

// Tests that signals can be collected when on a managed browser in an
// affiliated profile.
TEST_P(UserPermissionServiceImplTest,
       CanCollectSignals_BrowserManaged_AffiliatedUser) {
  SetDeviceAsCloudManaged();

  EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));
  EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(true));

  EXPECT_EQ(permission_service_->CanCollectSignals(), UserPermission::kGranted);
}

struct UnaffiliatedUserTestCase {
  bool machine_scope = false;
  bool user_scope = false;
  bool can_collect = false;
};

// Tests whether signals can be collected in various unaffiliated context
// use-cases.
TEST_P(UserPermissionServiceImplTest,
       CanCollectSignals_BrowserManaged_UnaffiliatedUser) {
  SetDeviceAsCloudManaged();

  const std::array<UnaffiliatedUserTestCase, 4> test_cases = {
      UnaffiliatedUserTestCase{/*machine_scope=*/false, /*user_scope=*/false,
                               /*can_collect=*/false},
      UnaffiliatedUserTestCase{/*machine_scope=*/false, /*user_scope=*/true,
                               /*can_collect=*/false},
      UnaffiliatedUserTestCase{/*machine_scope=*/true, /*user_scope=*/false,
                               /*can_collect=*/true},
      UnaffiliatedUserTestCase{/*machine_scope=*/true, /*user_scope=*/true,
                               /*can_collect=*/false},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_CALL(*mock_user_delegate_, IsManagedUser()).WillOnce(Return(true));
    EXPECT_CALL(*mock_user_delegate_, IsAffiliated()).WillOnce(Return(false));

    SetPolicyScopesNeedingSignals(test_case.machine_scope,
                                  test_case.user_scope);

    EXPECT_EQ(permission_service_->CanCollectSignals(),
              test_case.can_collect ? UserPermission::kGranted
                                    : UserPermission::kMissingConsent);
  }
}

INSTANTIATE_TEST_SUITE_P(, UserPermissionServiceImplTest, testing::Bool());

}  // namespace device_signals
