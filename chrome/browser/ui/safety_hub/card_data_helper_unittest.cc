// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/card_data_helper.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-shared.h"

using password_manager::BulkLeakCheckService;
using password_manager::TestPasswordStore;

namespace {

constexpr char kEmail[] = "test@example.com";

}  // namespace

class SafetyHubCardDataHelperTest : public testing::Test {
 public:
  SafetyHubCardDataHelperTest() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    // The profile that we create should use the proper testing factories that
    // encompass the identity test environment.
    profile_ = profile_manager_->CreateTestingProfile(
        kEmail, IdentityTestEnvironmentProfileAdaptor::
                    GetIdentityTestEnvironmentFactories());

    // Create an adaptor for the identity test environment, as we do not
    // directly control how the idenity test environment will be used.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_ = identity_test_env_adaptor_->identity_test_env();

    feature_list_.InitWithFeatures({features::kSafetyHub}, {});
    identity_test_env()->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSignin);

    profile_store_ = CreateAndUseTestPasswordStore(profile());
    account_store_ = CreateAndUseTestAccountPasswordStore(profile());
    bulk_leak_check_service_ =
        safety_hub_test_util::CreateAndUseBulkLeakCheckService(
            identity_test_env_->identity_manager(), profile());

    profile()->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().InSecondsFSinceUnixEpoch());
    SetMockCredentialEntry("https://example1.com", false);
    PasswordStatusCheckService* service =
        PasswordStatusCheckServiceFactory::GetForProfile(profile());
    service->UpdateInsecureCredentialCountAsync();
    RunUntilIdle();

    // Ensure that after the setup, the overall state is safe.
    ASSERT_EQ(safety_hub::GetOverallState(profile()),
              safety_hub::SafetyHubCardState::kSafe);
  }

  std::unique_ptr<KeyedService> SetMockCWSInfoService(
      content::BrowserContext* context) {
    return safety_hub_test_util::GetMockCWSInfoService(profile());
  }

 protected:
  void CreateMockNotificationPermissionForReview() {
    const GURL kUrl = GURL("https://www.example.com:443");
    auto* site_engagement_service =
        site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());

    // Set a host to have minimum engagement. This should be in review list.
    hcsm()->SetContentSettingDefaultScope(kUrl, GURL(),
                                          ContentSettingsType::NOTIFICATIONS,
                                          CONTENT_SETTING_ALLOW);
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());
    notifications_engagement_service->RecordNotificationDisplayed(kUrl, 7);

    site_engagement::SiteEngagementScore score =
        site_engagement_service->CreateEngagementScore(kUrl);
    score.Reset(0.5, base::Time::Now());
    score.Commit();
    EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
              site_engagement_service->GetEngagementLevel(kUrl));
  }

  void CreateMockUnusedSitePermissionForReview() {
    const GURL kUrl = GURL("https://www.example.com:443");
    base::Value::Dict dict;
    dict.Set(permissions::kRevokedKey,
             base::Value::List().Append(
                 UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
                     ContentSettingsType::GEOLOCATION)));
    content_settings::ContentSettingConstraints default_constraint(
        base::Time::Now());
    default_constraint.set_lifetime(base::Days(60));
    hcsm()->SetWebsiteSettingCustomScope(
        ContentSettingsPattern::FromURLNoWildcard(kUrl),
        ContentSettingsPattern::Wildcard(),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(std::move(dict)), default_constraint);
  }

  void SetMockCredentialEntry(const std::string origin,
                              bool leaked,
                              bool weak = false) {
    // Create a password form and mark it as leaked.
    const char16_t* password = weak ? u"123456" : u"Un1Qu3Cr3DeNt!aL";
    password_manager::PasswordForm form =
        safety_hub_test_util::MakeForm(u"username", password, origin, leaked);

    profile_store().AddLogin(form);
    account_store().AddLogin(form);
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestingProfile* profile() { return profile_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  raw_ptr<BulkLeakCheckService> bulk_leak_check_service_;
};

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_SafeBrowsingInfo) {
  // When Safe Browsing is disabled by admin, that card gets into the info
  // state. The overall state should thus be in the info state.
  prefs()->SetManagedPref(prefs::kSafeBrowsingEnabled, base::Value(false));
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kInfo);
}

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_NotificationPermission) {
  // When there are notification permissions to review, the overall state should
  // be "warning".
  auto* notification_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  CreateMockNotificationPermissionForReview();
  safety_hub_test_util::UpdateSafetyHubServiceAsync(notification_service);
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);
}

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_UnusedSitePermissions) {
  // When there are unused permissions to review, the overall state should
  // be "warning".
  auto* usp_service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile());
  CreateMockUnusedSitePermissionForReview();
  safety_hub_test_util::UpdateSafetyHubServiceAsync(usp_service);
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);
}

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_Extension) {
  // Create mock notifications that will require a review.
  extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&SafetyHubCardDataHelperTest::SetMockCWSInfoService,
                          base::Unretained(this)));
  safety_hub_test_util::CreateMockExtensions(profile());
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);
}

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_Password) {
  // Setting a weak password should result in a "weak" state.
  SetMockCredentialEntry("https://example1.com", /*leaked=*/false,
                         /*weak=*/true);
  PasswordStatusCheckService* psc_service =
      PasswordStatusCheckServiceFactory::GetForProfile(profile());
  psc_service->UpdateInsecureCredentialCountAsync();
  RunUntilIdle();

  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWeak);

  // When a leaked password is added, we should get in a "warning" state
  // instead.
  SetMockCredentialEntry("https://example2.com", /*leaked=*/true);
  psc_service->UpdateInsecureCredentialCountAsync();
  RunUntilIdle();
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);
}

TEST_F(SafetyHubCardDataHelperTest, GetOverallState_Multi) {
  // Initially we are in a "safe" state.
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kSafe);

  // Setting a weak password gets us in a "weak" state.
  SetMockCredentialEntry("https://example1.com", /*leaked=*/false,
                         /*weak=*/true);
  PasswordStatusCheckService* psc_service =
      PasswordStatusCheckServiceFactory::GetForProfile(profile());
  psc_service->UpdateInsecureCredentialCountAsync();
  RunUntilIdle();
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWeak);

  // When adding notification permissions to review, we should get into a
  // "warning" state.
  auto* notification_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  CreateMockNotificationPermissionForReview();
  safety_hub_test_util::UpdateSafetyHubServiceAsync(notification_service);
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);

  // When a leaked password is added, we should remain in the "warning" state.
  SetMockCredentialEntry("https://example2.com", /*leaked=*/true);
  psc_service->UpdateInsecureCredentialCountAsync();
  RunUntilIdle();
  EXPECT_EQ(safety_hub::GetOverallState(profile()),
            safety_hub::SafetyHubCardState::kWarning);
}
