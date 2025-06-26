// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_log_record.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

void PrintTo(SupervisedUserLogRecord::Segment segment, std::ostream* os) {
  switch (segment) {
    case SupervisedUserLogRecord::Segment::kUnsupervised:
      *os << "kUnsupervised";
      break;
    case SupervisedUserLogRecord::Segment::
        kSupervisionEnabledByFamilyLinkPolicy:
      *os << "kSupervisionEnabledByFamilyLinkPolicy";
      break;
    case SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkUser:
      *os << "kSupervisionEnabledByFamilyLinkUser";
      break;
    case SupervisedUserLogRecord::Segment::kMixedProfile:
      *os << "kMixedProfile";
      break;
    case SupervisedUserLogRecord::Segment::kParent:
      *os << "kParent";
      break;
    case SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally:
      *os << "kSupervisionEnabledLocally";
      break;
  }
}

namespace {
constexpr char kEmail[] = "name@gmail.com";

class SupervisedUserLogRecordTest : public ::testing::Test {
 public:
  SupervisedUserLogRecordTest() {
    supervised_user_test_environment_.pref_service_syncable()
        ->registry()
        ->RegisterBooleanPref(
            prefs::kSupervisedUserExtensionsMayRequestPermissions, false);
    supervised_user_test_environment_.pref_service_syncable()
        ->registry()
        ->RegisterBooleanPref(prefs::kSkipParentApprovalToInstallExtensions,
                              false);
    HostContentSettingsMap::RegisterProfilePrefs(
        supervised_user_test_environment_.pref_service_syncable()->registry());
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        supervised_user_test_environment_.pref_service(),
        /*is_off_the_record=*/false,
        /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
  }

  ~SupervisedUserLogRecordTest() override {
    host_content_settings_map_->ShutdownOnUIThread();
    supervised_user_test_environment_.Shutdown();
  }

  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    return &identity_test_env_;
  }

  std::unique_ptr<SupervisedUserLogRecord> CreateSupervisedUserLogRecord() {
    return std::make_unique<SupervisedUserLogRecord>(
        SupervisedUserLogRecord::Create(
            identity_test_env_.identity_manager(),
            *supervised_user_test_environment_.pref_service(),
            *host_content_settings_map_,
            supervised_user_test_environment_.service()));
  }

  // Creates a regular user account (most likely, an adult) with the given email
  // address.
  void CreateRegularUser() {
    AccountInfo account_info =
        GetIdentityTestEnv()->MakePrimaryAccountAvailable(
            kEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_fetch_family_member_info(true);
    mutator.set_is_subject_to_parental_controls(false);
    mutator.set_is_opted_in_to_parental_supervision(false);
    GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);
  }

  // Parent user is a regular (typically an adult) user with a family role.
  void CreateParentUser(kidsmanagement::FamilyRole family_role) {
    CreateRegularUser();
    supervised_user_test_environment_.pref_service()->SetString(
        prefs::kFamilyLinkUserMemberRole,
        supervised_user::FamilyRoleToString(family_role));
  }

  void CreateSupervisedUser(bool is_subject_to_parental_controls,
                            bool is_opted_in_to_parental_supervision) {
    AccountInfo account_info =
        GetIdentityTestEnv()->MakePrimaryAccountAvailable(
            kEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_fetch_family_member_info(true);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

    supervised_user::EnableParentalControls(
        *supervised_user_test_environment_.pref_service());
    // Set the Family Link `Permissions` switch to default value. In prod it's
    // done by the `SupervisedUserPrefStore`, but that requires fully
    // operational Profile.
    supervised_user_test_environment_.pref_service()->SetBoolean(
        prefs::kSupervisedUserExtensionsMayRequestPermissions, true);
  }

  std::unique_ptr<SupervisedUserLogRecord> CreateSupervisedUserWithWebFilter(
      WebFilterType web_filter_type) {
    CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                         /*is_opted_in_to_parental_supervision=*/false);
    supervised_user_test_environment_.SetWebFilterType(web_filter_type);

    return std::make_unique<SupervisedUserLogRecord>(
        SupervisedUserLogRecord::Create(
            identity_test_env_.identity_manager(),
            *supervised_user_test_environment_.pref_service(),
            *host_content_settings_map_,
            supervised_user_test_environment_.service()));
  }

#if BUILDFLAG(IS_ANDROID)
  void EnableSearchContentFilters() {
    supervised_user_test_environment_.search_content_filters_observer()
        ->SetEnabled(true);
  }

  void EnableBrowserContentFilters() {
    supervised_user_test_environment_.browser_content_filters_observer()
        ->SetEnabled(true);
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

TEST_F(SupervisedUserLogRecordTest, SignedOutIsUnsupervised) {
  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kUnsupervised);
}

TEST_F(SupervisedUserLogRecordTest, CapabilitiesUnknownDefault) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  EXPECT_FALSE(supervision_status.has_value());
}

TEST_F(SupervisedUserLogRecordTest, SupervisionEnabledByUser) {
  CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/true);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(
      supervision_status.value(),
      SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkUser);
}

TEST_F(SupervisedUserLogRecordTest, SupervisionEnabledByPolicy) {
  CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(
      supervision_status.value(),
      SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkPolicy);
}

TEST_F(SupervisedUserLogRecordTest, NotSupervised) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kUnsupervised);
}

TEST_F(SupervisedUserLogRecordTest, SignedOutHasNoWebFilter) {
  std::optional<WebFilterType> supervision_status =
      CreateSupervisedUserLogRecord()->GetWebFilterTypeForPrimaryAccount();
  EXPECT_FALSE(supervision_status.has_value());
}

TEST_F(SupervisedUserLogRecordTest, NotSupervisedHasNoWebFilter) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserLogRecord()->GetWebFilterTypeForPrimaryAccount();
  EXPECT_FALSE(web_filter.has_value());
}

TEST_F(SupervisedUserLogRecordTest, SupervisedWithMatureSitesFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kTryToBlockMatureSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  EXPECT_EQ(web_filter.value(), WebFilterType::kTryToBlockMatureSites);
}

TEST_F(SupervisedUserLogRecordTest, SupervisedWithAllowAllFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kAllowAllSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  EXPECT_EQ(web_filter.value(), WebFilterType::kAllowAllSites);
}

TEST_F(SupervisedUserLogRecordTest, SupervisedWithCertainSitesFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kCertainSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  EXPECT_EQ(web_filter.value(), WebFilterType::kCertainSites);
}

TEST_F(SupervisedUserLogRecordTest, HeadOfHousehold) {
  CreateParentUser(kidsmanagement::HEAD_OF_HOUSEHOLD);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kParent);
}

TEST_F(SupervisedUserLogRecordTest, Parent) {
  CreateParentUser(kidsmanagement::PARENT);

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kParent);
}

TEST_F(SupervisedUserLogRecordTest, RegularUserWithDisabledSupervision) {
  CreateRegularUser();
  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kUnsupervised);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SupervisedUserLogRecordTest, RegularUserWithSearchFilterEnabled) {
  CreateRegularUser();
  EnableSearchContentFilters();

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally);
}

TEST_F(SupervisedUserLogRecordTest, RegularUserWithContentFiltersEnabled) {
  CreateRegularUser();
  EnableBrowserContentFilters();

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally);
}

TEST_F(SupervisedUserLogRecordTest, RegularUserWithAllLocalFiltersEnabled) {
  CreateRegularUser();
  EnableSearchContentFilters();
  EnableBrowserContentFilters();

  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      CreateSupervisedUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  EXPECT_EQ(supervision_status.value(),
            SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally);
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace
}  // namespace supervised_user
