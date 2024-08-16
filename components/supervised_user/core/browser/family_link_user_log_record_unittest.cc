// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_user_log_record.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {
constexpr char kChildEmail[] = "name@gmail.com";
}  // namespace

class FamilyLinkUserLogRecordTest : public ::testing::Test {
 public:
  FamilyLinkUserLogRecordTest() {
    PrefRegistrySimple* registry = pref_service_.registry();
    supervised_user::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(
        prefs::kSupervisedUserExtensionsMayRequestPermissions, false);
    registry->RegisterBooleanPref(prefs::kSkipParentApprovalToInstallExtensions,
                                  false);
    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &pref_service_,
        /*is_off_the_record=*/false,
        /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
  }

  ~FamilyLinkUserLogRecordTest() override {
    host_content_settings_map_->ShutdownOnUIThread();
  }

  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    return &identity_test_env_;
  }

  std::unique_ptr<FamilyLinkUserLogRecord> CreateFamilyLinkUserLogRecord() {
    SupervisedUserURLFilter filter(pref_service_,
                                   std::make_unique<FakeURLFilterDelegate>());
    filter.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());

    return std::make_unique<FamilyLinkUserLogRecord>(
        FamilyLinkUserLogRecord::Create(identity_test_env_.identity_manager(),
                                        pref_service_,
                                        *host_content_settings_map_, &filter));
  }

  void CreateSupervisedUser(bool is_subject_to_parental_controls,
                            bool is_opted_in_to_parental_supervision) {
    AccountInfo account_info =
        GetIdentityTestEnv()->MakePrimaryAccountAvailable(
            kChildEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

    supervised_user::EnableParentalControls(pref_service_);
    // Set the Family Link `Permissions` switch to default value,
    // as done by the SupervisedUserPrefStore.
    pref_service_.SetBoolean(
        prefs::kSupervisedUserExtensionsMayRequestPermissions, true);
  }

  std::unique_ptr<FamilyLinkUserLogRecord> CreateSupervisedUserWithWebFilter(
      WebFilterType web_filter_type) {
    CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                         /*is_opted_in_to_parental_supervision=*/false);

    SupervisedUserURLFilter filter(pref_service_,
                                   std::make_unique<FakeURLFilterDelegate>());
    filter.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());

    switch (web_filter_type) {
      case WebFilterType::kAllowAllSites:
        pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, false);
        break;
      case WebFilterType::kTryToBlockMatureSites:
        pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, true);
        break;
      case WebFilterType::kCertainSites:
        filter.SetDefaultFilteringBehavior(
            supervised_user::FilteringBehavior::kBlock);
        break;
      case WebFilterType::kMixed:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    return std::make_unique<FamilyLinkUserLogRecord>(
        FamilyLinkUserLogRecord::Create(identity_test_env_.identity_manager(),
                                        pref_service_,
                                        *host_content_settings_map_, &filter));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

TEST_F(FamilyLinkUserLogRecordTest, SignedOutIsUnsupervised) {
  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kUnsupervised);
}

TEST_F(FamilyLinkUserLogRecordTest, CapabilitiesUnknownDefault) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_FALSE(supervision_status.has_value());
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisionEnabledByUser) {
  CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/true);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByUser);
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisionEnabledByPolicy) {
  CreateSupervisedUser(/*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByPolicy);
}

TEST_F(FamilyLinkUserLogRecordTest, NotSupervised) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kUnsupervised);
}

TEST_F(FamilyLinkUserLogRecordTest, SignedOutHasNoWebFilter) {
  std::optional<WebFilterType> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetWebFilterTypeForPrimaryAccount();
  ASSERT_FALSE(supervision_status.has_value());
}

TEST_F(FamilyLinkUserLogRecordTest, NotSupervisedHasNoWebFilter) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<WebFilterType> web_filter =
      CreateFamilyLinkUserLogRecord()->GetWebFilterTypeForPrimaryAccount();
  ASSERT_FALSE(web_filter.has_value());
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisedWithMatureSitesFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kTryToBlockMatureSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  ASSERT_EQ(web_filter.value(), WebFilterType::kTryToBlockMatureSites);
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisedWithAllowAllFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kAllowAllSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  ASSERT_EQ(web_filter.value(), WebFilterType::kAllowAllSites);
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisedWithCertainSitesFilter) {
  std::optional<WebFilterType> web_filter =
      CreateSupervisedUserWithWebFilter(WebFilterType::kCertainSites)
          ->GetWebFilterTypeForPrimaryAccount();
  ASSERT_TRUE(web_filter.has_value());
  ASSERT_EQ(web_filter.value(), WebFilterType::kCertainSites);
}

}  // namespace supervised_user
