// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/metrics_choice_handler.h"

#include <ranges>

#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/device_settings_cache.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

using ::testing::Eq;

// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* kOwner = policy::PolicyBuilder::kFakeUsername;
const char kNonOwner[] = "non@owner.com";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  StatsReportingController::RegisterLocalStatePrefs(local_state->registry());
  device_settings_cache::RegisterPrefs(local_state->registry());
  metrics::MetricsService::RegisterPrefs(local_state->registry());
  return local_state;
}

class TestUserMetricsServiceClient
    : public ::metrics::TestMetricsServiceClient {
 public:
  std::optional<bool> GetCurrentUserMetricsChoice() const override {
    if (should_use_user_consent_) {
      return current_user_choice_;
    }
    return std::nullopt;
  }

  void UpdateCurrentUserMetricsChoice(bool user_choice) override {
    current_user_choice_ = user_choice;
  }

  void SetShouldUseUserConsent(bool should_use_user_consent) {
    should_use_user_consent_ = should_use_user_consent;
  }

 private:
  bool should_use_user_consent_ = true;
  bool current_user_choice_ = false;
};

}  // namespace

class TestMetricsChoiceHandler : public MetricsChoiceHandler {
 public:
  TestMetricsChoiceHandler(Profile* profile,
                           metrics::MetricsService* metrics_service,
                           user_manager::UserManager* user_manager,
                           content::WebUI* web_ui)
      : MetricsChoiceHandler(profile, metrics_service, user_manager) {
    set_web_ui(web_ui);
  }
  ~TestMetricsChoiceHandler() override = default;

  void GetMetricsChoiceState() {
    base::ListValue args;
    args.Append("callback-id");
    HandleGetMetricsChoiceState(args);
  }

  void UpdateMetricsChoice(bool metrics_choice) {
    base::ListValue args;
    args.Append("callback-id");

    base::DictValue dict;
    dict.Set("consent", metrics_choice);
    args.Append(std::move(dict));

    HandleUpdateMetricsChoice(args);
  }
};

class MetricsChoiceHandlerTest : public testing::Test {
 public:
  MetricsChoiceHandlerTest() = default;
  MetricsChoiceHandlerTest(const MetricsChoiceHandlerTest&) = delete;
  MetricsChoiceHandlerTest& operator=(const MetricsChoiceHandlerTest&) = delete;
  ~MetricsChoiceHandlerTest() override = default;

  void RegisterOwner(const AccountId& account_id,
                     std::unique_ptr<TestingProfile>& owner) {
    DeviceSettingsService::Get()->StartProcessing(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &fake_session_manager_client_, owner_keys);
    owner = CreateUser(kOwner, owner_keys);
    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(account_id));
    user_manager::TestHelper::RegisterOwner(
        *TestingBrowserProcess::GetGlobal()->local_state(),
        account_id.GetUserEmail());
    user_manager::UserManager::Get()->SetOwnerId(account_id);

    EXPECT_THAT(DeviceSettingsService::Get()->GetOwnershipStatus(),
                Eq(DeviceSettingsService::OwnershipStatus::kOwnershipTaken));
  }

  void InitializeTestHandler(Profile* profile) {
    // Create the handler with given profile.
    handler_ = std::make_unique<TestMetricsChoiceHandler>(
        profile, test_metrics_service_.get(), user_manager::UserManager::Get(),
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
    test_user_session_manager_->LogIn(account_id);
  }

 protected:
  void SetUp() override {
    // Load device policy with owner.
    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    // Keys to be used for testing.
    non_owner_keys->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
    owner_keys->ImportPrivateKeyAndSetPublicKey(
        *device_policy_.GetSigningKey());

    content::RunAllTasksUntilIdle();

    StatsReportingController::Initialize(&pref_service_);

    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());
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

  void TearDown() override {
    handler_->DisallowJavascript();
    test_user_session_manager_.reset();
  }

  bool GetMetricsChoiceStateMessage(std::string* pref_name,
                                    bool* is_configurable) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         std::views::reverse(web_ui_->call_data())) {
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != "cr.webUIResponse" || !name ||
          *name != "callback-id") {
        continue;
      }

      if (!data->arg3() || !data->arg3()->is_dict()) {
        return false;
      }

      const base::DictValue& metrics_choice_state = data->arg3()->GetDict();
      *pref_name = *metrics_choice_state.FindString("prefName");
      *is_configurable = *metrics_choice_state.FindBool("isConfigurable");

      return true;
    }
    return false;
  }

  bool UpdateMetricsChoiceMessage(bool* current_choice) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         std::views::reverse(web_ui_->call_data())) {
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != "cr.webUIResponse" || !name ||
          *name != "callback-id") {
        continue;
      }

      if (!data->arg3() || data->arg3()->type() != base::Value::Type::BOOLEAN) {
        return false;
      }

      *current_choice = data->arg3()->GetBool();
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

  std::unique_ptr<TestMetricsChoiceHandler> handler_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
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

TEST_F(MetricsChoiceHandlerTest, OwnerCanToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, GaiaId("2"));
  std::unique_ptr<TestingProfile> owner;
  RegisterOwner(owner_id, owner);

  // Owner should not use user choice, but local pref.
  test_metrics_service_client_->SetShouldUseUserConsent(false);

  LoginUser(owner_id);
  EXPECT_TRUE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  InitializeTestHandler(owner.get());
  handler_->GetMetricsChoiceState();

  // Owner should be able to toggle the device stats reporting pref.
  std::string pref_name;
  bool is_configurable = false;

  // Owner should be able to toggle the device stats reporting pref.
  EXPECT_TRUE(GetMetricsChoiceStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(kStatsReportingPref, Eq(pref_name));
  EXPECT_TRUE(is_configurable);

  // Toggle true. Choice change should go through.
  handler_->UpdateMetricsChoice(true);

  bool current_choice = false;
  EXPECT_TRUE(UpdateMetricsChoiceMessage(&current_choice));

  // Choice should change for owner.
  EXPECT_TRUE(current_choice);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsChoiceHandlerTest, NonOwnerWithUserConsentCanToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, GaiaId("2"));
  std::unique_ptr<TestingProfile> owner;
  RegisterOwner(owner_id, owner);

  auto non_owner_id = AccountId::FromUserEmailGaiaId(kNonOwner, GaiaId("1"));
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, non_owner_keys);
  ASSERT_TRUE(test_user_session_manager_->AddRegularUser(non_owner_id));

  // User should use user choice pref.
  test_metrics_service_client_->SetShouldUseUserConsent(true);

  LoginUser(non_owner_id);
  EXPECT_FALSE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  InitializeTestHandler(non_owner.get());
  handler_->GetMetricsChoiceState();

  std::string pref_name;
  bool is_configurable = false;

  // Non-owner user should use user choice.
  EXPECT_TRUE(GetMetricsChoiceStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(pref_name, Eq(::metrics::prefs::kMetricsUserConsent));
  EXPECT_TRUE(is_configurable);

  // Toggle true.
  handler_->UpdateMetricsChoice(true);

  bool current_choice = false;
  EXPECT_TRUE(UpdateMetricsChoiceMessage(&current_choice));

  // Choice should change.
  EXPECT_TRUE(current_choice);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsChoiceHandlerTest, NonOwnerWithoutUserConsentCannotToggle) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, GaiaId("2"));
  std::unique_ptr<TestingProfile> owner;
  RegisterOwner(owner_id, owner);

  auto non_owner_id = AccountId::FromUserEmailGaiaId(kNonOwner, GaiaId("1"));
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, non_owner_keys);
  ASSERT_TRUE(test_user_session_manager_->AddRegularUser(non_owner_id));

  // User cannot use user choice. This happens if the device is managed.
  test_metrics_service_client_->SetShouldUseUserConsent(false);

  LoginUser(non_owner_id);
  EXPECT_FALSE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  InitializeTestHandler(non_owner.get());
  handler_->GetMetricsChoiceState();

  std::string pref_name;
  bool is_configurable = false;

  // Display device choice.
  EXPECT_TRUE(GetMetricsChoiceStateMessage(&pref_name, &is_configurable));
  EXPECT_THAT(kStatsReportingPref, Eq(pref_name));
  EXPECT_FALSE(is_configurable);

  // Try to toggle true.
  handler_->UpdateMetricsChoice(true);

  bool current_choice = false;
  EXPECT_TRUE(UpdateMetricsChoiceMessage(&current_choice));

  // Choice should not change.
  EXPECT_FALSE(current_choice);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

TEST_F(MetricsChoiceHandlerTest, ChildUserCannotToggleAsNonOwner) {
  auto owner_id = AccountId::FromUserEmailGaiaId(kOwner, GaiaId("2"));
  std::unique_ptr<TestingProfile> owner;
  RegisterOwner(owner_id, owner);

  auto child_id = AccountId::FromUserEmailGaiaId("child@user.com", GaiaId("3"));
  std::unique_ptr<TestingProfile> child =
      CreateUser("child@user.com", non_owner_keys);
  ASSERT_TRUE(test_user_session_manager_->AddChildUser(child_id));

  // User cannot use user choice. This happens if the device is managed.
  test_metrics_service_client_->SetShouldUseUserConsent(true);

  LoginUser(child_id);
  EXPECT_FALSE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  // Set the javascript message object for metrics choice state.
  InitializeTestHandler(child.get());
  handler_->GetMetricsChoiceState();

  // Check values of javascript callback response message.
  std::string pref_name;
  bool is_configurable;
  EXPECT_TRUE(GetMetricsChoiceStateMessage(&pref_name, &is_configurable));

  // Unmanaged child user should use user choice and should not be toggle-able.
  EXPECT_THAT(pref_name, Eq(::metrics::prefs::kMetricsUserConsent));
  EXPECT_FALSE(is_configurable);

  // Explicitly shutdown controller here because OwnerSettingsService is
  // destructed before TearDown() is called.
  StatsReportingController::Shutdown();
}

}  // namespace ash::settings
