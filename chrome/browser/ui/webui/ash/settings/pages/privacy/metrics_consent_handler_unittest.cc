// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/metrics_consent_handler.h"

#include "base/containers/adapters.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
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

namespace ash::settings {

namespace {

using ::testing::Eq;

// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* kOwner = policy::PolicyBuilder::kFakeUsername;
constexpr char kNonOwner[] = "non@owner.com";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  StatsReportingController::RegisterLocalStatePrefs(local_state->registry());
  device_settings_cache::RegisterPrefs(local_state->registry());
  metrics::MetricsService::RegisterPrefs(local_state->registry());
  return local_state;
}

class TestUserMetricsServiceClient
    : public ::metrics::TestMetricsServiceClient {
 public:
  std::optional<bool> GetCurrentUserMetricsConsent() const override {
    if (should_use_user_consent_)
      return current_user_metrics_consent_;
    return std::nullopt;
  }

  void UpdateCurrentUserMetricsConsent(bool metrics_consent) override {
    current_user_metrics_consent_ = metrics_consent;
  }

  void SetShouldUseUserConsent(bool should_use_user_consent) {
    should_use_user_consent_ = should_use_user_consent;
  }

 private:
  bool should_use_user_consent_ = true;
  bool current_user_metrics_consent_ = false;
};

}  // namespace

class TestMetricsConsentHandler : public MetricsConsentHandler {
 public:
  TestMetricsConsentHandler(Profile* profile,
                            metrics::MetricsService* metrics_service,
                            user_manager::UserManager* user_manager,
                            content::WebUI* web_ui)
      : MetricsConsentHandler(profile, metrics_service, user_manager) {
    set_web_ui(web_ui);
  }
  ~TestMetricsConsentHandler() override = default;

  void GetMetricsConsentState() {
    base::Value::List args;
    args.Append("callback-id");
    HandleGetMetricsConsentState(args);
  }

  void UpdateMetricsConsent(bool metrics_consent) {
    base::Value::List args;
    args.Append("callback-id");

    base::Value::Dict dict;
    dict.Set("consent", metrics_consent);
    args.Append(std::move(dict));

    HandleUpdateMetricsConsent(args);
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
    std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, owner_keys);
    test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false, user_manager::UserType::kRegular, owner.get());
    test_user_manager_->SetOwnerId(account_id);

    EXPECT_THAT(DeviceSettingsService::Get()->GetOwnershipStatus(),
                Eq(DeviceSettingsService::OwnershipStatus::kOwnershipTaken));

    return owner;
  }

  void InitializeTestHandler(Profile* profile) {
    // Create the handler with given profile.
    handler_ = std::make_unique<TestMetricsConsentHandler>(
        profile, test_metrics_service_.get(), test_user_manager_.get(),
        web_ui_.get());

    // Enable javascript.
    handler_->AllowJavascriptForTesting();
  }

  std::unique_ptr<TestingProfile> CreateUser(
      const char* username,
      scoped_refptr<ownership::MockOwnerKeyUtil> keys) {
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        keys);

    TestingProfile::Builder builder;
    builder.SetProfileName(username);
    std::unique_ptr<TestingProfile> user = builder.Build();

    FakeNssService::InitializeForBrowserContext(user.get(),
                                                /*enable_system_slot=*/false);

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
    owner_keys->ImportPrivateKeyAndSetPublicKey(device_policy_.GetSigningKey());

    content::RunAllTasksUntilIdle();

    StatsReportingController::Initialize(&pref_service_);

    test_user_manager_ = std::make_unique<FakeChromeUserManager>();
    web_ui_ = std::make_unique<content::TestWebUI>();

    test_enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(true, true);
    test_metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, test_enabled_state_provider_.get(), std::wstring(),
        base::FilePath());
    test_metrics_service_client_ =
        std::make_unique<TestUserMetricsServiceClient>();
    test_metrics_service_ = std::make_unique<metrics::MetricsService>(
        test_metrics_state_manager_.get(), test_metrics_service_client_.get(),
        &pref_service_);

    // Needs to be set for metrics service.
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
  }

  void TearDown() override { handler_->DisallowJavascript(); }

  bool GetMetricsConsentStateMessage(std::string* pref_name,
                                     bool* is_configurable) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_->call_data())) {
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != "cr.webUIResponse" || !name ||
          *name != "callback-id") {
        continue;
      }

      if (!data->arg3() || !data->arg3()->is_dict()) {
        return false;
      }

      const base::Value::Dict& metrics_consent_state = data->arg3()->GetDict();
      *pref_name = *metrics_consent_state.FindString("prefName");
      *is_configurable = *metrics_consent_state.FindBool("isConfigurable");

      return true;
    }
    return false;
  }

  bool UpdateMetricsConsentMessage(bool* current_consent) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_->call_data())) {
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

  // Set up stubs for StatsReportingController.
  ScopedStubInstallAttributes scoped_install_attributes_;
  FakeSessionManagerClient fake_session_manager_client_;
  ScopedTestDeviceSettingsService scoped_device_settings_;
  CrosSettingsHolder cros_settings_holder_{ash::DeviceSettingsService::Get(),
                                           RegisterPrefs(&pref_service_)};

  std::unique_ptr<TestMetricsConsentHandler> handler_;
  std::unique_ptr<FakeChromeUserManager> test_user_manager_;
  std::unique_ptr<content::TestWebUI> web_ui_;

  // MetricsService.
  // Dangling Pointer Prevention: test_enabled_state_provider_ must be listed
  // before test_metrics_state_manager_ to avoid a dangling pointer.
  std::unique_ptr<metrics::TestEnabledStateProvider>
      test_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> test_metrics_state_manager_;
  std::unique_ptr<TestUserMetricsServiceClient> test_metrics_service_client_;
  std::unique_ptr<metrics::MetricsService> test_metrics_service_;

  policy::DevicePolicyBuilder device_policy_;

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> non_owner_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
};

TEST_F(MetricsConsentHandlerTest, OwnerCanToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  // Owner should not use user consent, but local pref.
  test_metrics_service_client_->SetShouldUseUserConsent(false);

  LoginUser(owner_id);
  EXPECT_TRUE(test_user_manager_->IsCurrentUserOwner());

  InitializeTestHandler(owner.get());
  handler_->GetMetricsConsentState();

  // Owner should be able to toggle the device stats reporting pref.
  std::string pref_name;
  bool is_configurable = false;

  // Owner should be able to toggle the device stats reporting pref.
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(kStatsReportingPref, Eq(pref_name));
  EXPECT_TRUE(is_configurable);

  // Toggle true. Consent change should go through.
  handler_->UpdateMetricsConsent(true);

  bool current_consent = false;
  EXPECT_TRUE(UpdateMetricsConsentMessage(&current_consent));

  // Consent should change for owner.
  EXPECT_TRUE(current_consent);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsConsentHandlerTest, NonOwnerWithUserConsentCanToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  auto non_owner_id = AccountId::FromUserEmailGaiaId(kNonOwner, "1");
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, non_owner_keys);
  test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      non_owner_id, false, user_manager::UserType::kRegular, non_owner.get());

  // User should use user consent pref.
  test_metrics_service_client_->SetShouldUseUserConsent(true);

  LoginUser(non_owner_id);
  EXPECT_FALSE(test_user_manager_->IsCurrentUserOwner());

  InitializeTestHandler(non_owner.get());
  handler_->GetMetricsConsentState();

  std::string pref_name;
  bool is_configurable = false;

  // Non-owner user should use user consent.
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(pref_name, Eq(::metrics::prefs::kMetricsUserConsent));
  EXPECT_TRUE(is_configurable);

  // Toggle true.
  handler_->UpdateMetricsConsent(true);

  bool current_consent = false;
  EXPECT_TRUE(UpdateMetricsConsentMessage(&current_consent));

  // Consent should change.
  EXPECT_TRUE(current_consent);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsConsentHandlerTest, NonOwnerWithoutUserConsentCannotToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  auto non_owner_id = AccountId::FromUserEmailGaiaId(kNonOwner, "1");
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, non_owner_keys);
  test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      non_owner_id, false, user_manager::UserType::kRegular, non_owner.get());

  // User cannot use user consent. This happens if the device is managed.
  test_metrics_service_client_->SetShouldUseUserConsent(false);

  LoginUser(non_owner_id);
  EXPECT_FALSE(test_user_manager_->IsCurrentUserOwner());

  InitializeTestHandler(non_owner.get());
  handler_->GetMetricsConsentState();

  std::string pref_name;
  bool is_configurable = false;

  // Display device consent.
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(kStatsReportingPref, Eq(pref_name));
  EXPECT_FALSE(is_configurable);

  // Try to toggle true.
  handler_->UpdateMetricsConsent(true);

  bool current_consent = false;
  EXPECT_TRUE(UpdateMetricsConsentMessage(&current_consent));

  // Consent should not change.
  EXPECT_FALSE(current_consent);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsConsentHandlerTest, ChildUserCannotToggleAsNonOwner) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, "2");
  std::unique_ptr<TestingProfile> owner = RegisterOwner(owner_id);

  auto child_id = AccountId::FromUserEmailGaiaId("child@user.com", "3");
  std::unique_ptr<TestingProfile> child =
      CreateUser("child@user.com", non_owner_keys);
  test_user_manager_->set_current_user_child(true);
  test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      child_id, false, user_manager::UserType::kChild, child.get());

  // User cannot use user consent. This happens if the device is managed.
  test_metrics_service_client_->SetShouldUseUserConsent(true);

  LoginUser(child_id);
  EXPECT_FALSE(test_user_manager_->IsCurrentUserOwner());

  // Set the javascript message object for metrics consent state.
  InitializeTestHandler(child.get());
  handler_->GetMetricsConsentState();

  // Check values of javascript callback response message.
  std::string pref_name;
  bool is_configurable;
  EXPECT_TRUE(GetMetricsConsentStateMessage(&pref_name, &is_configurable));

  // Unmanaged child user should use user consent and should not be toggle-able.
  EXPECT_THAT(pref_name, Eq(::metrics::prefs::kMetricsUserConsent));
  EXPECT_FALSE(is_configurable);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

}  // namespace ash::settings
