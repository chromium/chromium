// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/metrics_consent_handler.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

using ::testing::Eq;

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  StatsReportingController::RegisterLocalStatePrefs(local_state->registry());
  ash::device_settings_cache::RegisterPrefs(local_state->registry());
  return local_state;
}

}  // namespace

class TestMetricsConsentHandler : public MetricsConsentHandler {
 public:
  TestMetricsConsentHandler(Profile* profile,
                            user_manager::UserManager* user_manager,
                            content::WebUI* web_ui)
      : MetricsConsentHandler(profile, user_manager) {
    set_web_ui(web_ui);
  }
  ~TestMetricsConsentHandler() override = default;

  void GetMetricsConsentState() {
    base::ListValue args;
    args.Append(base::Value("callback-id"));
    HandleGetMetricsConsentState(args.GetList());
  }

  void UpdateMetricsConsent(bool metrics_consent) {
    base::ListValue args;
    args.Append(base::Value("callback-id"));

    base::DictionaryValue dict;
    dict.SetBoolKey("consent", metrics_consent);
    args.Append(std::move(dict));

    HandleUpdateMetricsConsent(args.GetList());
  }
};

class MetricsConsentHandlerTest : public testing::Test {
 public:
  MetricsConsentHandlerTest() = default;
  MetricsConsentHandlerTest(const MetricsConsentHandlerTest&) = delete;
  MetricsConsentHandlerTest& operator=(const MetricsConsentHandlerTest&) =
      delete;
  ~MetricsConsentHandlerTest() override = default;

  std::unique_ptr<TestingProfile> RegisterOwner(const AccountId& account_id) {
    DeviceSettingsService::Get()->SetSessionManager(
        &fake_session_manager_client_, owner_keys);
    std::unique_ptr<TestingProfile> owner = CreateUser(owner_keys);
    test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false, user_manager::USER_TYPE_REGULAR, owner.get());
    test_user_manager_->SetOwnerId(account_id);

    EXPECT_THAT(ash::DeviceSettingsService::Get()->GetOwnershipStatus(),
                Eq(ash::DeviceSettingsService::OWNERSHIP_TAKEN));

    return owner;
  }

  void InitializeTestHandler(Profile* profile) {
    // Create the handler with given profile.
    handler_ = std::make_unique<TestMetricsConsentHandler>(
        profile, test_user_manager_.get(), web_ui_.get());

    // Enable javascript.
    handler_->AllowJavascriptForTesting();
  }

  std::unique_ptr<TestingProfile> CreateUser(
      scoped_refptr<ownership::MockOwnerKeyUtil> keys) {
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        keys);
    std::unique_ptr<TestingProfile> user = std::make_unique<TestingProfile>();
    OwnerSettingsServiceAshFactory::GetForBrowserContext(user.get())
        ->OnTPMTokenReady();
    content::RunAllTasksUntilIdle();
    return user;
  }

  void LoginUser(const AccountId& account_id) {
    test_user_manager_->LoginUser(account_id);
    test_user_manager_->SwitchActiveUser(account_id);
    test_user_manager_->SimulateUserProfileLoad(account_id);
  }

 protected:
  void SetUp() override {
    // Load device policy with owner.
    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    // Keys to be used for testing.
    non_owner_keys->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
    owner_keys->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
    owner_keys->SetPrivateKey(device_policy_.GetSigningKey());

    content::RunAllTasksUntilIdle();

    ash::StatsReportingController::Initialize(&pref_service_);

    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    web_ui_ = std::make_unique<content::TestWebUI>();
  }

  void TearDown() override { handler_->DisallowJavascript(); }

  bool GetMetricsConsentStateMessage(std::string* pref_name,
                                     bool* is_configurable) {
    for (auto it = web_ui_->call_data().rbegin();
         it != web_ui_->call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != "cr.webUIResponse" || !name ||
          *name != "callback-id") {
        continue;
      }

      if (!data->arg3() ||
          data->arg3()->type() != base::Value::Type::DICTIONARY) {
        return false;
      }

      const base::Value* metrics_consent_state = data->arg3();
      *pref_name = metrics_consent_state->FindKey("prefName")->GetString();
      *is_configurable =
          metrics_consent_state->FindKey("isConfigurable")->GetBool();

      return true;
    }
    return false;
  }

  bool UpdateMetricsConsentMessage(bool* current_consent) {
    for (auto it = web_ui_->call_data().rbegin();
         it != web_ui_->call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != "cr.webUIResponse" || !name ||
          *name != "callback-id") {
        continue;
      }

      if (!data->arg3() || data->arg3()->type() != base::Value::Type::BOOLEAN) {
        return false;
      }

      *current_consent = data->arg3()->GetBool();
      return true;
    }
    return false;
  }

  // Profiles must be created in browser threads.
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<TestMetricsConsentHandler> handler_;
  std::unique_ptr<ash::FakeChromeUserManager> test_user_manager_;
  std::unique_ptr<content::TestWebUI> web_ui_;

  // Set up stubs for StatsReportingController.
  chromeos::ScopedStubInstallAttributes scoped_install_attributes_;
  ash::FakeSessionManagerClient fake_session_manager_client_;
  ash::ScopedTestDeviceSettingsService scoped_device_settings_;
  ash::ScopedTestCrosSettings scoped_cros_settings_{
      RegisterPrefs(&pref_service_)};
  policy::DevicePolicyBuilder device_policy_;

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> non_owner_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
};

TEST_F(MetricsConsentHandlerTest, OwnerCanToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId("owner@example.com", "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  LoginUser(owner_id);
  EXPECT_TRUE(test_user_manager_->IsCurrentUserOwner());

  InitializeTestHandler(owner.get());
  handler_->GetMetricsConsentState();

  // Owner should be able to toggle the device stats reporting pref.
  std::string pref_name;
  bool is_configurable = false;

  // Non-owner user should not be able to toggle the device stats reporting
  // pref.
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(::ash::kStatsReportingPref, Eq(pref_name));
  EXPECT_TRUE(is_configurable);

  // Toggle true. Consent change should go through.
  handler_->UpdateMetricsConsent(true);

  bool current_consent = false;
  EXPECT_TRUE(UpdateMetricsConsentMessage(&current_consent));

  // Consent should change for owner.
  EXPECT_TRUE(current_consent);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  ash::StatsReportingController::Shutdown();
}

TEST_F(MetricsConsentHandlerTest, NonOwnerCannotToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId("owner@example.com", "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  auto account_id = AccountId::FromUserEmailGaiaId("test@example.com", "1");
  std::unique_ptr<TestingProfile> non_owner = CreateUser(non_owner_keys);
  test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      account_id, false, user_manager::USER_TYPE_REGULAR, non_owner.get());

  LoginUser(account_id);
  EXPECT_FALSE(test_user_manager_->IsCurrentUserOwner());

  InitializeTestHandler(non_owner.get());
  handler_->GetMetricsConsentState();

  std::string pref_name;
  bool is_configurable = false;

  // Non-owner user should not be able to toggle the device stats reporting
  // pref.
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(::ash::kStatsReportingPref, Eq(pref_name));
  EXPECT_FALSE(is_configurable);

  // Toggle true.
  handler_->UpdateMetricsConsent(true);

  bool current_consent = false;
  EXPECT_TRUE(UpdateMetricsConsentMessage(&current_consent));

  // Consent should not change.
  EXPECT_FALSE(current_consent);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  ash::StatsReportingController::Shutdown();
}

}  // namespace settings
}  // namespace chromeos
