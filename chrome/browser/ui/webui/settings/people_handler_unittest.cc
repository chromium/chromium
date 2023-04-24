// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Const;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Values;

namespace {

const char kTestUser[] = "chrome_p13n_test@gmail.com";
const char kTestCallbackId[] = "test-callback-id";

// Returns a UserSelectableTypeSet with all types set.
syncer::UserSelectableTypeSet GetAllTypes() {
  return syncer::UserSelectableTypeSet::All();
}

enum SyncAllDataConfig {
  SYNC_ALL_DATA,
  CHOOSE_WHAT_TO_SYNC
};

// Create a json-format string with the key/value pairs appropriate for a call
// to HandleSetDatatypes().
std::string GetConfiguration(SyncAllDataConfig sync_all,
                             syncer::UserSelectableTypeSet types) {
  base::Value::Dict result;
  result.Set("syncAllDataTypes", sync_all == SYNC_ALL_DATA);
  // Add all of our data types.
  result.Set("appsSynced", types.Has(syncer::UserSelectableType::kApps));
  result.Set("autofillSynced",
             types.Has(syncer::UserSelectableType::kAutofill));
  result.Set("bookmarksSynced",
             types.Has(syncer::UserSelectableType::kBookmarks));
  result.Set("extensionsSynced",
             types.Has(syncer::UserSelectableType::kExtensions));
  result.Set("passwordsSynced",
             types.Has(syncer::UserSelectableType::kPasswords));
  result.Set("preferencesSynced",
             types.Has(syncer::UserSelectableType::kPreferences));
  result.Set("readingListSynced",
             types.Has(syncer::UserSelectableType::kReadingList));
  result.Set("savedTabGroupsSynced",
             types.Has(syncer::UserSelectableType::kSavedTabGroups));
  result.Set("tabsSynced", types.Has(syncer::UserSelectableType::kTabs));
  result.Set("themesSynced", types.Has(syncer::UserSelectableType::kThemes));
  result.Set("typedUrlsSynced",
             types.Has(syncer::UserSelectableType::kHistory));
  result.Set("wifiConfigurationsSynced",
             types.Has(syncer::UserSelectableType::kWifiConfigurations));
  result.Set("paymentsIntegrationEnabled", false);

  // Reading list doesn't really have a UI and is supported on ios only.
  result.Set("readingListSynced",
             types.Has(syncer::UserSelectableType::kReadingList));

  std::string args;
  base::JSONWriter::Write(result, &args);
  return args;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|. This will fail if the key isn't present, even if
// |expected_value| is false.
void ExpectHasBoolKey(const base::Value::Dict& dictionary,
                      const std::string& key,
                      bool expected_value) {
  ASSERT_TRUE(dictionary.contains(key)) << "No value found for " << key;
  ASSERT_TRUE(dictionary.FindBool(key).has_value()) << key << " has wrong type";
  EXPECT_EQ(expected_value, *dictionary.FindBool(key))
      << "Mismatch found for " << key;
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the showSyncSetupPage() JS function for a given set of data
// types.
void CheckConfigDataTypeArguments(const base::Value::Dict& dictionary,
                                  SyncAllDataConfig config,
                                  syncer::UserSelectableTypeSet types) {
  ExpectHasBoolKey(dictionary, "syncAllDataTypes", config == SYNC_ALL_DATA);
  ExpectHasBoolKey(dictionary, "appsSynced",
                   types.Has(syncer::UserSelectableType::kApps));
  ExpectHasBoolKey(dictionary, "autofillSynced",
                   types.Has(syncer::UserSelectableType::kAutofill));
  ExpectHasBoolKey(dictionary, "bookmarksSynced",
                   types.Has(syncer::UserSelectableType::kBookmarks));
  ExpectHasBoolKey(dictionary, "extensionsSynced",
                   types.Has(syncer::UserSelectableType::kExtensions));
  ExpectHasBoolKey(dictionary, "passwordsSynced",
                   types.Has(syncer::UserSelectableType::kPasswords));
  ExpectHasBoolKey(dictionary, "preferencesSynced",
                   types.Has(syncer::UserSelectableType::kPreferences));
  ExpectHasBoolKey(dictionary, "readingListSynced",
                   types.Has(syncer::UserSelectableType::kReadingList));
  ExpectHasBoolKey(dictionary, "savedTabGroupsSynced",
                   types.Has(syncer::UserSelectableType::kSavedTabGroups));
  ExpectHasBoolKey(dictionary, "tabsSynced",
                   types.Has(syncer::UserSelectableType::kTabs));
  ExpectHasBoolKey(dictionary, "themesSynced",
                   types.Has(syncer::UserSelectableType::kThemes));
  ExpectHasBoolKey(dictionary, "typedUrlsSynced",
                   types.Has(syncer::UserSelectableType::kHistory));
  ExpectHasBoolKey(dictionary, "wifiConfigurationsSynced",
                   types.Has(syncer::UserSelectableType::kWifiConfigurations));
}

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
}

}  // namespace

namespace settings {

class TestingPeopleHandler : public PeopleHandler {
 public:
  TestingPeopleHandler(content::WebUI* web_ui, Profile* profile)
      : PeopleHandler(profile) {
    set_web_ui(web_ui);
  }

  TestingPeopleHandler(const TestingPeopleHandler&) = delete;
  TestingPeopleHandler& operator=(const TestingPeopleHandler&) = delete;

  using PeopleHandler::is_configuring_sync;

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point) override {}
#endif
};

class TestWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    return std::make_unique<content::WebUIController>(web_ui);
  }
};

class PeopleHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  PeopleHandlerTest() = default;

  PeopleHandlerTest(const PeopleHandlerTest&) = delete;
  PeopleHandlerTest& operator=(const PeopleHandlerTest&) = delete;

  ~PeopleHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSyncService)));

    ON_CALL(*mock_sync_service_, HasSyncConsent()).WillByDefault(Return(true));

    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
        .WillByDefault(Return(syncer::PassphraseType::kImplicitPassphrase));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            GetExplicitPassphraseTime())
        .WillByDefault(Return(base::Time()));
    ON_CALL(*mock_sync_service_, GetSetupInProgressHandle())
        .WillByDefault(
            Return(ByMove(std::make_unique<syncer::SyncSetupInProgressHandle>(
                mock_on_setup_in_progress_handle_destroyed_.Get()))));
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_->DisallowJavascript();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SigninUser() {
    identity_test_env()->SetPrimaryAccount(kTestUser,
                                           signin::ConsentLevel::kSync);
  }

  void CreatePeopleHandler() {
    handler_ = std::make_unique<TestingPeopleHandler>(&web_ui_, profile());
    handler_->AllowJavascript();
    web_ui_.set_web_contents(web_contents());
  }

  // Setup the expectations for calls made when displaying the config page.
  void SetDefaultExpectationsForConfigPage() {
    ON_CALL(*mock_sync_service_, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            GetRegisteredSelectableTypes())
        .WillByDefault(Return(GetAllTypes()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsSyncEverythingEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(Return(GetAllTypes()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsCustomPassphraseAllowed())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsEncryptEverythingEnabled())
        .WillByDefault(Return(false));
  }

  void SetupInitializedSyncService() {
    // An initialized SyncService will have already completed sync setup and
    // will have an initialized sync engine.
    ON_CALL(*mock_sync_service_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void ExpectPageStatusResponse(const std::string& expected_status) {
    auto& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool());
    ASSERT_TRUE(data.arg3()->is_string());
    EXPECT_EQ(expected_status, data.arg3()->GetString());
  }

  // Expects a call to ResolveJavascriptCallback() with |should_succeed| as its
  // argument.
  void ExpectSetPassphraseSuccess(bool should_succeed) {
    EXPECT_EQ(1u, web_ui_.call_data().size());
    const auto& data = *web_ui_.call_data()[0];
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool())
        << "Callback should be resolved with a boolean indicating the success, "
           "never rejected.";

    EXPECT_TRUE(data.arg3()->is_bool());
    EXPECT_EQ(should_succeed, data.arg3()->GetBool());
  }

  std::vector<const base::Value*> GetAllFiredValuesForEventName(
      const std::string& event_name) {
    std::vector<const base::Value*> arguments;
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         web_ui_.call_data()) {
      if (data->function_name() == "cr.webUIListenerCallback" &&
          data->arg1()->is_string() &&
          data->arg1()->GetString() == event_name) {
        arguments.push_back(data->arg2());
      }
    }
    return arguments;
  }

  // Must be called at most once per test to check if a sync-prefs-changed
  // event happened. Returns the single fired value.
  base::Value::Dict ExpectSyncPrefsChanged() {
    std::vector<const base::Value*> args =
        GetAllFiredValuesForEventName("sync-prefs-changed");
    EXPECT_EQ(1U, args.size());
    EXPECT_NE(args[0], nullptr);
    EXPECT_TRUE(args[0]->is_dict());
    return args[0]->GetDict().Clone();
  }

  // Must be called at most once per test to check if a sync-status-changed
  // event happened. Returns the single fired value.
  base::Value::Dict ExpectSyncStatusChanged() {
    std::vector<const base::Value*> args =
        GetAllFiredValuesForEventName("sync-status-changed");
    EXPECT_EQ(1U, args.size());
    EXPECT_NE(args[0], nullptr);
    EXPECT_TRUE(args[0]->is_dict());
    return args[0]->GetDict().Clone();
  }

  void NotifySyncStateChanged() {
    handler_->OnStateChanged(mock_sync_service_);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  testing::NiceMock<base::MockCallback<base::RepeatingClosure>>
      mock_on_setup_in_progress_handle_destroyed_;
  raw_ptr<syncer::MockSyncService> mock_sync_service_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  content::TestWebUI web_ui_;
  TestWebUIProvider test_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
  std::unique_ptr<TestingPeopleHandler> handler_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PeopleHandlerTest, DisplayBasicLogin) {
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      ConsentLevel::kSync));
  CreatePeopleHandler();
  // Test that the HandleStartSignin call enables JavaScript.
  handler_->DisallowJavascript();

  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Ensure that the user is not signed in before calling |HandleStartSignin()|.
  identity_test_env()->ClearPrimaryAccount();
  handler_->HandleStartSignin(base::Value::List());

  // Sync setup hands off control to the gaia login tab.
  EXPECT_EQ(
      nullptr,
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());

  ASSERT_FALSE(handler_->is_configuring_sync());

  handler_->CloseSyncSetup();
  EXPECT_EQ(
      nullptr,
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PeopleHandlerTest, DisplayConfigureWithEngineDisabledAndCancel) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested());

  // We're simulating a user setting up sync, which would cause the engine to
  // kick off initialization, but not download user data types. The sync
  // engine will try to download control data types (e.g encryption info), but
  // that won't finish for this test as we're simulating cancelling while the
  // spinner is showing.
  handler_->HandleShowSyncSetupUI(base::Value::List());

  EXPECT_EQ(
      handler_.get(),
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());

  EXPECT_EQ(0U, web_ui_.call_data().size());

  handler_->CloseSyncSetup();
  EXPECT_EQ(
      nullptr,
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());
}

// Verifies that the handler only sends the sync pref updates once the engine is
// initialized.
TEST_F(PeopleHandlerTest,
       DisplayConfigureWithEngineDisabledAndSyncStartupCompleted) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
  // Sync engine is stopped initially, and will start up.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested());
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(base::Value::List());

  // No data is sent yet, because the engine is not initialized.
  EXPECT_EQ(0U, web_ui_.call_data().size());

  Mock::VerifyAndClearExpectations(mock_sync_service_);
  // Now, act as if the SyncService has started up.
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  NotifySyncStateChanged();

  // Updates for the sync status, sync prefs and trusted vault opt-in are sent.
  EXPECT_EQ(3U, web_ui_.call_data().size());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "syncAllDataTypes", true);
  ExpectHasBoolKey(dictionary, "customPassphraseAllowed", true);
  ExpectHasBoolKey(dictionary, "encryptAllData", false);
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
  ExpectHasBoolKey(dictionary, "trustedVaultKeysRequired", false);
}

// Verifies the case where the user cancels after the sync engine has
// initialized.
TEST_F(PeopleHandlerTest,
       DisplayConfigureWithEngineDisabledAndCancelAfterSigninSuccess) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_sync_service_, GetTransportState())
      .WillOnce(Return(syncer::SyncService::TransportState::INITIALIZING))
      .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested());
  SetDefaultExpectationsForConfigPage();
  handler_->HandleShowSyncSetupUI(base::Value::List());

  // Sync engine becomes active, so |handler_| is notified.
  NotifySyncStateChanged();

  // It's important to tell sync the user cancelled the setup flow before we
  // tell it we're through with the setup progress.
  testing::InSequence seq;
  EXPECT_CALL(*mock_sync_service_, StopAndClear());
  EXPECT_CALL(mock_on_setup_in_progress_handle_destroyed_, Run());

  handler_->CloseSyncSetup();
  EXPECT_EQ(
      nullptr,
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());
}

TEST_F(PeopleHandlerTest, RestartSyncAfterDashboardClear) {
  SigninUser();
  CreatePeopleHandler();
  // Clearing sync from the dashboard results in DISABLE_REASON_USER_CHOICE
  // being set.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));

  // Attempting to open the setup UI should restart sync.
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested())
      .WillOnce([&]() {
        // SetSyncFeatureRequested() clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::INITIALIZING));
      });

  handler_->HandleShowSyncSetupUI(base::Value::List());

  // Since the engine is not initialized yet, no data should be sent.
  EXPECT_EQ(0U, web_ui_.call_data().size());
}

TEST_F(PeopleHandlerTest,
       RestartSyncAfterDashboardClearWithStandaloneTransport) {
  SigninUser();
  CreatePeopleHandler();
  // Clearing sync from the dashboard results in DISABLE_REASON_USER_CHOICE
  // being set. However, the sync engine has restarted in standalone transport
  // mode.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));

  // Attempting to open the setup UI should re-enable sync-the-feature.
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested())
      .WillOnce([&]() {
        // SetSyncFeatureRequested() clears DISABLE_REASON_USER_CHOICE. Since
        // the engine is already running, it just gets reconfigured.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::CONFIGURING));
      });

  handler_->HandleShowSyncSetupUI(base::Value::List());
  // Since the engine was already running, we should *not* get a spinner - all
  // the necessary values are already available.
  ExpectSyncPrefsChanged();
}

// Tests that signals not related to user intention to configure sync don't
// trigger sync engine start.
TEST_F(PeopleHandlerTest, OnlyStartEngineWhenConfiguringSync) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested()).Times(0);
  NotifySyncStateChanged();
}

TEST_F(PeopleHandlerTest, AcquireSyncBlockerWhenLoadingSyncSettingsSubpage) {
  SigninUser();
  CreatePeopleHandler();
  // We set up a factory override here to prevent a new web ui from being
  // created when we navigate to a page that would normally create one.
  TestChromeWebUIControllerFactory test_factory;
  content::ScopedWebUIControllerFactoryRegistration factory_registration(
      &test_factory, ChromeWebUIControllerFactory::GetInstance());
  test_factory.AddFactoryOverride(
      chrome::GetSettingsUrl(chrome::kSyncSetupSubPage).host(),
      &test_provider_);

  EXPECT_FALSE(handler_->sync_blocker_);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      chrome::GetSettingsUrl(chrome::kSyncSetupSubPage), web_contents());
  navigation->Start();
  handler_->InitializeSyncBlocker();

  EXPECT_TRUE(handler_->sync_blocker_);
}

TEST_F(PeopleHandlerTest, UnrecoverableErrorInitializingSync) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(
          Return(syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Open the web UI.
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(PeopleHandlerTest, GaiaErrorInitializingSync) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Open the web UI.
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(PeopleHandlerTest, TestSyncEverything) {
  SigninUser();
  CreatePeopleHandler();
  std::string args = GetConfiguration(SYNC_ALL_DATA, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(true, _));
  handler_->HandleSetDatatypes(list_args);

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, EnterCorrectExistingPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase("correct_passphrase"))
      .WillOnce(Return(true));

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("correct_passphrase");
  handler_->HandleSetDecryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(true);
}

TEST_F(PeopleHandlerTest, SuccessfullyCreateCustomPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase("custom_passphrase"));

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("custom_passphrase");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(true);
}

TEST_F(PeopleHandlerTest, EnterWrongExistingPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase("invalid_passphrase"))
      .WillOnce(Return(false));

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("invalid_passphrase");
  handler_->HandleSetDecryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);
}

TEST_F(PeopleHandlerTest, CannotCreateBlankPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase)
      .Times(0);

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(PeopleHandlerTest, TestSyncIndividualTypes) {
  SigninUser();
  CreatePeopleHandler();
  SetDefaultExpectationsForConfigPage();
  for (syncer::UserSelectableType type : GetAllTypes()) {
    syncer::UserSelectableTypeSet type_to_set;
    type_to_set.Put(type);
    std::string args = GetConfiguration(CHOOSE_WHAT_TO_SYNC, type_to_set);
    base::Value::List list_args;
    list_args.Append(kTestCallbackId);
    list_args.Append(args);
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsPassphraseRequiredForPreferredDataTypes())
        .WillByDefault(Return(false));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(false));
    SetupInitializedSyncService();
    EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
                SetSelectedTypes(false, type_to_set));

    handler_->HandleSetDatatypes(list_args);
    ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
  }
}

TEST_F(PeopleHandlerTest, TestSyncAllManually) {
  SigninUser();
  CreatePeopleHandler();
  SetDefaultExpectationsForConfigPage();
  std::string args = GetConfiguration(CHOOSE_WHAT_TO_SYNC, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(false, GetAllTypes()));
  handler_->HandleSetDatatypes(list_args);

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, NonRegisteredType) {
  SigninUser();
  CreatePeopleHandler();
  SetDefaultExpectationsForConfigPage();

  // Simulate apps not being registered.
  syncer::UserSelectableTypeSet registered_types = GetAllTypes();
  registered_types.Remove(syncer::UserSelectableType::kApps);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          GetRegisteredSelectableTypes())
      .WillByDefault(Return(registered_types));
  SetupInitializedSyncService();

  // Simulate "Sync everything" being turned off, but all individual
  // toggles left on.
  std::string config = GetConfiguration(CHOOSE_WHAT_TO_SYNC, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(config);

  // Only the registered types are selected.
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(/*sync_everything=*/false, registered_types));
  handler_->HandleSetDatatypes(list_args);
}

TEST_F(PeopleHandlerTest, ShowSyncSetup) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  // This should display the sync setup dialog (not login).
  SetDefaultExpectationsForConfigPage();
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ExpectSyncPrefsChanged();
}

TEST_F(PeopleHandlerTest, ShowSetupSyncEverything) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "syncAllDataTypes", true);
  ExpectHasBoolKey(dictionary, "appsRegistered", true);
  ExpectHasBoolKey(dictionary, "autofillRegistered", true);
  ExpectHasBoolKey(dictionary, "bookmarksRegistered", true);
  ExpectHasBoolKey(dictionary, "extensionsRegistered", true);
  ExpectHasBoolKey(dictionary, "passwordsRegistered", true);
  ExpectHasBoolKey(dictionary, "preferencesRegistered", true);
  ExpectHasBoolKey(dictionary, "readingListRegistered", true);
  ExpectHasBoolKey(dictionary, "tabsRegistered", true);
  ExpectHasBoolKey(dictionary, "themesRegistered", true);
  ExpectHasBoolKey(dictionary, "typedUrlsRegistered", true);
  ExpectHasBoolKey(dictionary, "paymentsIntegrationEnabled", true);
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
  ExpectHasBoolKey(dictionary, "encryptAllData", false);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_DATA, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupManuallySyncAll) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncEverythingEnabled())
      .WillByDefault(Return(false));
  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  SigninUser();
  CreatePeopleHandler();
  for (syncer::UserSelectableType type : GetAllTypes()) {
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(false));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsUsingExplicitPassphrase())
        .WillByDefault(Return(false));
    SetupInitializedSyncService();
    SetDefaultExpectationsForConfigPage();
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsSyncEverythingEnabled())
        .WillByDefault(Return(false));
    syncer::UserSelectableTypeSet types(type);
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(Return(types));

    // This should display the sync setup dialog (not login).
    handler_->HandleShowSyncSetupUI(base::Value::List());

    // Close the config overlay.
    LoginUIServiceFactory::GetForProfile(profile())->LoginUIClosed(
        handler_.get());

    base::Value::Dict dictionary = ExpectSyncPrefsChanged();
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
    // Clean up so we can loop back to display the dialog again.
    web_ui_.ClearTrackedCalls();
  }
}

TEST_F(PeopleHandlerTest, ShowSetupOldGaiaPassphraseRequired) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  const auto passphrase_time = base::Time::Now();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          GetExplicitPassphraseTime())
      .WillByDefault(Return(passphrase_time));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kFrozenImplicitPassphrase));

  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", true);
  ASSERT_TRUE(dictionary.contains("explicitPassphraseTime"));
  ASSERT_TRUE(dictionary.FindString("explicitPassphraseTime"));
  EXPECT_EQ(base::UTF16ToUTF8(base::TimeFormatShortDate(passphrase_time)),
            *dictionary.FindString("explicitPassphraseTime"));
}

TEST_F(PeopleHandlerTest, ShowSetupCustomPassphraseRequired) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  const auto passphrase_time = base::Time::Now();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          GetExplicitPassphraseTime())
      .WillByDefault(Return(passphrase_time));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kCustomPassphrase));

  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", true);
  ASSERT_TRUE(dictionary.contains("explicitPassphraseTime"));
  ASSERT_TRUE(dictionary.FindString("explicitPassphraseTime"));
  EXPECT_EQ(base::UTF16ToUTF8(base::TimeFormatShortDate(passphrase_time)),
            *dictionary.FindString("explicitPassphraseTime"));
}

TEST_F(PeopleHandlerTest, ShowSetupTrustedVaultKeysRequired) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kTrustedVaultPassphrase));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
  ExpectHasBoolKey(dictionary, "trustedVaultKeysRequired", true);
  EXPECT_FALSE(dictionary.contains("explicitPassphraseTime"));
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAll) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsEncryptEverythingEnabled())
      .WillByDefault(Return(true));

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "encryptAllData", true);
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAllDisallowed) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(false));

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "encryptAllData", false);
  ExpectHasBoolKey(dictionary, "customPassphraseAllowed", false);
}

TEST_F(PeopleHandlerTest, CannotCreatePassphraseIfCustomPassphraseDisallowed) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase)
      .Times(0);

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("passphrase123");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);
}

TEST_F(PeopleHandlerTest, CannotOverwritePassphraseWithNewOne) {
  SigninUser();
  CreatePeopleHandler();
  SetupInitializedSyncService();

  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingExplicitPassphrase())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsCustomPassphraseAllowed())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase)
      .Times(0);

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("passphrase123");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmSoon) {
  SigninUser();
  CreatePeopleHandler();
  // Sync starts out fully enabled.
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(base::Value::List());

  // Now sync gets reset from the dashboard (the user clicked the "Manage synced
  // data" link), which results in the sync-requested and first-setup-complete
  // bits being cleared.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Sync will eventually start again in transport mode.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));

  NotifySyncStateChanged();

  // Now the user confirms sync again. This should set both the sync-requested
  // and the first-setup-complete bits.
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested())
      .WillOnce([&]() {
        // SetSyncFeatureRequested() clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::INITIALIZING));
        NotifySyncStateChanged();
      });
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetFirstSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM))
      .WillOnce([&](syncer::SyncFirstSetupCompleteSource) {
        ON_CALL(*mock_sync_service_->GetMockUserSettings(),
                IsFirstSetupComplete())
            .WillByDefault(Return(true));
        NotifySyncStateChanged();
      });

  base::Value::List did_abort;
  did_abort.Append(false);
  handler_->OnDidClosePage(did_abort);
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmLater) {
  SigninUser();
  CreatePeopleHandler();
  // Sync starts out fully enabled.
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(base::Value::List());

  // Now sync gets reset from the dashboard (the user clicked the "Manage synced
  // data" link), which results in the sync-requested and first-setup-complete
  // bits being cleared.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Sync will eventually start again in transport mode.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));

  NotifySyncStateChanged();

  // The user waits a while before doing anything, so sync starts up in
  // transport mode.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  // On some platforms (e.g. ChromeOS), the first-setup-complete bit gets set
  // automatically during engine startup.
  if (browser_defaults::kSyncAutoStarts) {
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
        .WillByDefault(Return(true));
  }
  NotifySyncStateChanged();

  // Now the user confirms sync again. This should set the sync-requested bit
  // and (if it wasn't automatically set above already) also the
  // first-setup-complete bit.
  EXPECT_CALL(*mock_sync_service_, SetSyncFeatureRequested())
      .WillOnce([&]() {
        // SetSyncFeatureRequested() clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::INITIALIZING));
        NotifySyncStateChanged();
      });
  if (!browser_defaults::kSyncAutoStarts) {
    EXPECT_CALL(
        *mock_sync_service_->GetMockUserSettings(),
        SetFirstSetupComplete(
            syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM))
        .WillOnce([&](syncer::SyncFirstSetupCompleteSource) {
          ON_CALL(*mock_sync_service_->GetMockUserSettings(),
                  IsFirstSetupComplete())
              .WillByDefault(Return(true));
          NotifySyncStateChanged();
        });
  }

  base::Value::List did_abort;
  did_abort.Append(false);
  handler_->OnDidClosePage(did_abort);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(PeopleHandlerDiceUnifiedConsentTest, StoredAccountsList) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  // Do not be in first run, so that the profiles are not created as "new
  // profiles" and automatically migrated to Dice.
  first_run::ResetCachedSentinelDataForTesting();
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindOnce(&first_run::ResetCachedSentinelDataForTesting));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  ASSERT_FALSE(first_run::IsChromeFirstRun());

  content::BrowserTaskEnvironment task_environment;

  // Setup the profile.
  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment();
  ASSERT_EQ(true, AccountConsistencyModeManager::IsDiceEnabledForProfile(
                      profile.get()));

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();

  auto account_1 = identity_test_env->MakeAccountAvailable("a@gmail.com");
  auto account_2 = identity_test_env->MakeAccountAvailable("b@gmail.com");
  identity_test_env->SetPrimaryAccount(account_1.email,
                                       signin::ConsentLevel::kSignin);

  PeopleHandler handler(profile.get());
  base::Value::List accounts = handler.GetStoredAccountsList();

  ASSERT_EQ(2u, accounts.size());
  ASSERT_TRUE(accounts[0].GetDict().FindString("email"));
  ASSERT_TRUE(accounts[1].GetDict().FindString("email"));
  EXPECT_EQ("a@gmail.com", *accounts[0].GetDict().FindString("email"));
  EXPECT_EQ("b@gmail.com", *accounts[1].GetDict().FindString("email"));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(PeopleHandlerMainProfile, Signout) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(
              builder, signin::AccountConsistencyMethod::kMirror);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  auto* identity_manager = identity_test_env->identity_manager();

  identity_test_env->MakePrimaryAccountAvailable("user@gmail.com",
                                                 ConsentLevel::kSync);
  ASSERT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));

  identity_test_env->MakeAccountAvailable("a@gmail.com");
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());

  PeopleHandler handler(profile.get());
  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  handler.HandleSignout(args);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  // Signout should only revoke sync consent and not change any accounts.
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());
}

#if DCHECK_IS_ON()
TEST(PeopleHandlerMainProfile, DeleteProfileCrashes) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(
              builder, signin::AccountConsistencyMethod::kMirror);

  PeopleHandler handler(profile.get());
  base::Value::List args;
  args.Append(/*delete_profile=*/true);
  EXPECT_DEATH(handler.HandleSignout(args), ".*");
}
#endif  // DCHECK_IS_ON()

TEST(PeopleHandlerSecondaryProfile, SignoutWhenSyncing) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(false);

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(
              builder, signin::AccountConsistencyMethod::kMirror);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  auto* identity_manager = identity_test_env->identity_manager();

  auto account_1 = identity_test_env->MakeAccountAvailable("a@gmail.com");
  auto account_2 = identity_test_env->MakeAccountAvailable("b@gmail.com");
  identity_test_env->SetPrimaryAccount(account_1.email,
                                       signin::ConsentLevel::kSync);
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());

  PeopleHandler handler(profile.get());
  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  handler.HandleSignout(args);
  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager->GetAccountsWithRefreshTokens().empty());
}

TEST(PeopleHandlerMainProfile, GetStoredAccountsList) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(
              builder, signin::AccountConsistencyMethod::kMirror);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  auto* identity_manager = identity_test_env->identity_manager();

  identity_test_env->MakePrimaryAccountAvailable("user@gmail.com",
                                                 ConsentLevel::kSignin);
  ASSERT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));

  identity_test_env->MakeAccountAvailable("a@gmail.com");
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());

  PeopleHandler handler(profile.get());
  base::Value::List accounts = handler.GetStoredAccountsList();

  ASSERT_EQ(1u, accounts.size());
  ASSERT_TRUE(accounts[0].GetDict().FindString("email"));
  EXPECT_EQ("user@gmail.com", *accounts[0].GetDict().FindString("email"));
}

TEST(PeopleHandlerSecondaryProfile, GetStoredAccountsList) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(false);

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(
              builder, signin::AccountConsistencyMethod::kMirror);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  auto* identity_manager = identity_test_env->identity_manager();

  auto account_1 = identity_test_env->MakeAccountAvailable("a@gmail.com");
  auto account_2 = identity_test_env->MakeAccountAvailable("b@gmail.com");
  identity_test_env->SetPrimaryAccount(account_2.email,
                                       signin::ConsentLevel::kSignin);
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());

  PeopleHandler handler(profile.get());
  base::Value::List accounts = handler.GetStoredAccountsList();

  ASSERT_EQ(2u, accounts.size());
  ASSERT_TRUE(accounts[0].GetDict().FindString("email"));
  ASSERT_TRUE(accounts[1].GetDict().FindString("email"));
  EXPECT_EQ(account_2.email, *accounts[0].GetDict().FindString("email"));
  EXPECT_EQ(account_1.email, *accounts[1].GetDict().FindString("email"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Regression test for crash in guest mode. https://crbug.com/1040476
TEST(PeopleHandlerGuestModeTest, GetStoredAccountsList) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<Profile> profile = builder.Build();

  PeopleHandler handler(profile.get());
  base::Value::List accounts = handler.GetStoredAccountsList();
  EXPECT_TRUE(accounts.empty());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PeopleHandlerTest, TurnOffSync) {
  // Simulate a user who previously turned on sync.
  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com",
                                                   ConsentLevel::kSync);
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  CreatePeopleHandler();
  handler_->HandleTurnOffSync(base::Value::List());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  base::Value::Dict status = ExpectSyncStatusChanged();
  ExpectHasBoolKey(status, "signedIn", false);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PeopleHandlerTest, GetStoredAccountsList) {
  // Chrome OS sets an unconsented primary account on login.
  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com",
                                                   ConsentLevel::kSignin);
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  CreatePeopleHandler();
  base::Value::List accounts = handler_->GetStoredAccountsList();
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ("user@gmail.com", *accounts[0].GetDict().FindString("email"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
class PeopleHandlerSignoutTest : public BrowserWithTestWindowTest {
 public:
  PeopleHandlerSignoutTest() = default;
  ~PeopleHandlerSignoutTest() override = default;

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  PeopleHandler* handler() { return handler_.get(); }

  void CreatePeopleHandler() {
    handler_ = std::make_unique<TestingPeopleHandler>(&web_ui_, profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
    web_ui_.set_web_contents(web_contents());
  }

  SigninClient* GetSigninSlient(Profile* profile) {
    return ChromeSigninClientFactory::GetForProfile(profile);
  }

 private:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories(
            signin::AccountConsistencyMethod::kDice);
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_->DisallowJavascript();
    identity_test_env_profile_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingPeopleHandler> handler_;
};

#if DCHECK_IS_ON()
TEST_F(PeopleHandlerSignoutTest, RevokeSyncNotAllowed) {
  // Ensure |PrimaryAccountMutatorImpl::RevokeSyncConsent| would not call
  // 'ClearPrimaryAccount' which breaks the test.
  EXPECT_NE(AccountConsistencyModeManager::GetMethodForProfile(profile()),
            signin::AccountConsistencyMethod::kDisabled);

  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  GetSigninSlient(profile())->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);

  CreatePeopleHandler();
  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  EXPECT_DEATH(handler()->HandleSignout(args), ".*");
}

TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOff) {
  // Ensure |PrimaryAccountMutatorImpl::RevokeSyncConsent| would not call
  // 'ClearPrimaryAccount' which breaks the test.
  EXPECT_NE(AccountConsistencyModeManager::GetMethodForProfile(profile()),
            signin::AccountConsistencyMethod::kDisabled);

  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  GetSigninSlient(profile())->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  EXPECT_DEATH(handler()->HandleSignout(args), ".*");
}
#endif  // DCHECK_IS_ON()

TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOn) {
  // Ensure |PrimaryAccountMutatorImpl::RevokeSyncConsent| would not call
  // 'ClearPrimaryAccount' which breaks the test.
  EXPECT_NE(AccountConsistencyModeManager::GetMethodForProfile(profile()),
            signin::AccountConsistencyMethod::kDisabled);

  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());
  GetSigninSlient(profile())->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  EXPECT_TRUE(ChromeSigninClientFactory::GetForProfile(profile())
                  ->IsRevokeSyncConsentAllowed());

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  handler()->HandleSignout(args);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  // Signout not triggered on dice platforms.
  EXPECT_EQ(web_contents()->GetVisibleURL().spec(), chrome::kChromeUINewTabURL);
  EXPECT_NE(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->service_logout_url());
}

TEST_F(PeopleHandlerSignoutTest, SignoutWithSyncOff) {
  // Ensure |PrimaryAccountMutatorImpl::RevokeSyncConsent| would not call
  // 'ClearPrimaryAccount' which breaks the test.
  EXPECT_NE(AccountConsistencyModeManager::GetMethodForProfile(profile()),
            signin::AccountConsistencyMethod::kDisabled);

  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  handler()->HandleSignout(args);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->service_logout_url());
#else
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

TEST_F(PeopleHandlerSignoutTest, SignoutWithSyncOn) {
  // Ensure |PrimaryAccountMutatorImpl::RevokeSyncConsent| would not call
  // 'ClearPrimaryAccount' which breaks the test.
  EXPECT_NE(AccountConsistencyModeManager::GetMethodForProfile(profile()),
            signin::AccountConsistencyMethod::kDisabled);

  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  EXPECT_NE(handler()->web_ui(), nullptr);
  EXPECT_NE(nullptr, handler()->web_ui()->GetWebContents());

  EXPECT_TRUE(chrome::FindBrowserWithWebContents(
      handler()->web_ui()->GetWebContents()));

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  handler()->HandleSignout(args);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->service_logout_url());
#else
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace settings
