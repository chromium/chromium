// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/driver/sync_user_settings_mock.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
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

enum EncryptAllConfig {
  ENCRYPT_ALL_DATA,
  ENCRYPT_PASSWORDS
};

// Create a json-format string with the key/value pairs appropriate for a call
// to HandleSetEncryption(). If |extra_values| is non-null, then the values from
// the passed dictionary are added to the json.
std::string GetConfiguration(const base::DictionaryValue* extra_values,
                             SyncAllDataConfig sync_all,
                             syncer::UserSelectableTypeSet types,
                             const std::string& passphrase,
                             EncryptAllConfig encrypt_all) {
  base::DictionaryValue result;
  if (extra_values)
    result.MergeDictionary(extra_values);
  result.SetBoolean("syncAllDataTypes", sync_all == SYNC_ALL_DATA);
  result.SetBoolean("encryptAllData", encrypt_all == ENCRYPT_ALL_DATA);
  if (!passphrase.empty())
    result.SetString("passphrase", passphrase);
  // Add all of our data types.
  result.SetBoolean("appsSynced", types.Has(syncer::UserSelectableType::kApps));
  result.SetBoolean("autofillSynced",
                    types.Has(syncer::UserSelectableType::kAutofill));
  result.SetBoolean("bookmarksSynced",
                    types.Has(syncer::UserSelectableType::kBookmarks));
  result.SetBoolean("extensionsSynced",
                    types.Has(syncer::UserSelectableType::kExtensions));
  result.SetBoolean("passwordsSynced",
                    types.Has(syncer::UserSelectableType::kPasswords));
  result.SetBoolean("preferencesSynced",
                    types.Has(syncer::UserSelectableType::kPreferences));
  result.SetBoolean("tabsSynced", types.Has(syncer::UserSelectableType::kTabs));
  result.SetBoolean("themesSynced",
                    types.Has(syncer::UserSelectableType::kThemes));
  result.SetBoolean("typedUrlsSynced",
                    types.Has(syncer::UserSelectableType::kHistory));
  result.SetBoolean("wifiConfigurationsSynced",
                    types.Has(syncer::UserSelectableType::kWifiConfigurations));
  result.SetBoolean("paymentsIntegrationEnabled", false);

  // Reading list doesn't really have a UI and is supported on ios only.
  result.SetBoolean("readingListSynced",
                    types.Has(syncer::UserSelectableType::kReadingList));

  std::string args;
  base::JSONWriter::Write(result, &args);
  return args;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|. If |omit_if_false| is true, then the value should only
// be present if |expected_value| is true.
void CheckBool(const base::DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value,
               bool omit_if_false) {
  if (omit_if_false && !expected_value) {
    EXPECT_FALSE(dictionary->HasKey(key)) <<
        "Did not expect to find value for " << key;
  } else {
    bool actual_value;
    EXPECT_TRUE(dictionary->GetBoolean(key, &actual_value)) <<
        "No value found for " << key;
    EXPECT_EQ(expected_value, actual_value) <<
        "Mismatch found for " << key;
  }
}

void CheckBool(const base::DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value) {
  return CheckBool(dictionary, key, expected_value, false);
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the showSyncSetupPage() JS function for a given set of data
// types.
void CheckConfigDataTypeArguments(const base::DictionaryValue* dictionary,
                                  SyncAllDataConfig config,
                                  syncer::UserSelectableTypeSet types) {
  CheckBool(dictionary, "syncAllDataTypes", config == SYNC_ALL_DATA);
  CheckBool(dictionary, "appsSynced",
            types.Has(syncer::UserSelectableType::kApps));
  CheckBool(dictionary, "autofillSynced",
            types.Has(syncer::UserSelectableType::kAutofill));
  CheckBool(dictionary, "bookmarksSynced",
            types.Has(syncer::UserSelectableType::kBookmarks));
  CheckBool(dictionary, "extensionsSynced",
            types.Has(syncer::UserSelectableType::kExtensions));
  CheckBool(dictionary, "passwordsSynced",
            types.Has(syncer::UserSelectableType::kPasswords));
  CheckBool(dictionary, "preferencesSynced",
            types.Has(syncer::UserSelectableType::kPreferences));
  CheckBool(dictionary, "tabsSynced",
            types.Has(syncer::UserSelectableType::kTabs));
  CheckBool(dictionary, "themesSynced",
            types.Has(syncer::UserSelectableType::kThemes));
  CheckBool(dictionary, "typedUrlsSynced",
            types.Has(syncer::UserSelectableType::kHistory));
  CheckBool(dictionary, "wifiConfigurationsSynced",
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

  using PeopleHandler::is_configuring_sync;

 private:
#if !defined(OS_CHROMEOS)
  void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point) override {}
#endif

  DISALLOW_COPY_AND_ASSIGN(TestingPeopleHandler);
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
  ~PeopleHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSyncService)));

    ON_CALL(*mock_sync_service_, IsAuthenticatedAccountPrimary())
        .WillByDefault(Return(true));

    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
        .WillByDefault(Return(syncer::PassphraseType::kImplicitPassphrase));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            GetExplicitPassphraseTime())
        .WillByDefault(Return(base::Time()));
    ON_CALL(*mock_sync_service_, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
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

  void SigninUser() { identity_test_env()->SetPrimaryAccount(kTestUser); }

  void CreatePeopleHandler() {
    handler_ = std::make_unique<TestingPeopleHandler>(&web_ui_, profile());
    handler_->AllowJavascript();
    web_ui_.set_web_contents(web_contents());
  }

  // Setup the expectations for calls made when displaying the config page.
  void SetDefaultExpectationsForConfigPage() {
    ON_CALL(*mock_sync_service_, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            GetRegisteredSelectableTypes())
        .WillByDefault(Return(GetAllTypes()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsSyncEverythingEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(Return(GetAllTypes()));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsEncryptEverythingAllowed())
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
    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(kTestCallbackId, callback_id);
    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    EXPECT_TRUE(success);
    std::string status;
    ASSERT_TRUE(data.arg3()->GetAsString(&status));
    EXPECT_EQ(expected_status, status);
  }

  const base::DictionaryValue* ExpectSyncPrefsChanged() {
    const content::TestWebUI::CallData& data1 = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data1.function_name());

    std::string event;
    EXPECT_TRUE(data1.arg1()->GetAsString(&event));
    EXPECT_EQ(event, "sync-prefs-changed");

    const base::DictionaryValue* dictionary = nullptr;
    EXPECT_TRUE(data1.arg2()->GetAsDictionary(&dictionary));
    return dictionary;
  }

  const base::DictionaryValue* ExpectSyncStatusChanged() {
    const content::TestWebUI::CallData& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    std::string event;
    EXPECT_TRUE(data.arg1()->GetAsString(&event));
    EXPECT_EQ(event, "sync-status-changed");

    const base::DictionaryValue* dictionary = nullptr;
    EXPECT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
    return dictionary;
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
  syncer::MockSyncService* mock_sync_service_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  content::TestWebUI web_ui_;
  TestWebUIProvider test_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
  std::unique_ptr<TestingPeopleHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(PeopleHandlerTest);
};

#if !defined(OS_CHROMEOS)
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
  base::ListValue list_args;
  handler_->HandleStartSignin(&list_args);

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

#endif  // !defined(OS_CHROMEOS)

TEST_F(PeopleHandlerTest, DisplayConfigureWithEngineDisabledAndCancel) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true));

  // We're simulating a user setting up sync, which would cause the engine to
  // kick off initialization, but not download user data types. The sync
  // engine will try to download control data types (e.g encryption info), but
  // that won't finish for this test as we're simulating cancelling while the
  // spinner is showing.
  handler_->HandleShowSyncSetupUI(nullptr);

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
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
      .WillByDefault(Return(true));
  // Sync engine is stopped initially, and will start up.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true));
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(nullptr);

  // No data is sent yet, because the engine is not initialized.
  EXPECT_EQ(0U, web_ui_.call_data().size());

  Mock::VerifyAndClearExpectations(mock_sync_service_);
  // Now, act as if the SyncService has started up.
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  NotifySyncStateChanged();

  // Updates for the sync status and the sync prefs are sent.
  EXPECT_EQ(2U, web_ui_.call_data().size());

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "encryptAllDataAllowed", true);
  CheckBool(dictionary, "encryptAllData", false);
  CheckBool(dictionary, "passphraseRequired", false);
  CheckBool(dictionary, "trustedVaultKeysRequired", false);
}

// Verifies the case where the user cancels after the sync engine has
// initialized.
TEST_F(PeopleHandlerTest,
       DisplayConfigureWithEngineDisabledAndCancelAfterSigninSuccess) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_sync_service_, GetTransportState())
      .WillOnce(Return(syncer::SyncService::TransportState::INITIALIZING))
      .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true));
  SetDefaultExpectationsForConfigPage();
  handler_->HandleShowSyncSetupUI(nullptr);

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
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true))
      .WillOnce([&](bool) {
        // SetSyncRequested(true) clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
            .WillByDefault(Return(true));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::INITIALIZING));
      });

  handler_->HandleShowSyncSetupUI(nullptr);

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
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true))
      .WillOnce([&](bool) {
        // SetSyncRequested(true) clears DISABLE_REASON_USER_CHOICE. Since the
        // engine is already running, it just gets reconfigured.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
            .WillByDefault(Return(true));
        ON_CALL(*mock_sync_service_, GetTransportState())
            .WillByDefault(
                Return(syncer::SyncService::TransportState::CONFIGURING));
      });

  handler_->HandleShowSyncSetupUI(nullptr);
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
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true))
      .Times(0);
  NotifySyncStateChanged();
}

TEST_F(PeopleHandlerTest, AcquireSyncBlockerWhenLoadingSyncSettingsSubpage) {
  SigninUser();
  CreatePeopleHandler();
  // We set up a factory override here to prevent a new web ui from being
  // created when we navigate to a page that would normally create one.
  test_factory_ = std::make_unique<TestChromeWebUIControllerFactory>();
  test_factory_->AddFactoryOverride(
      chrome::GetSettingsUrl(chrome::kSyncSetupSubPage).host(),
      &test_provider_);
  content::WebUIControllerFactory::RegisterFactory(test_factory_.get());
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      ChromeWebUIControllerFactory::GetInstance());

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
  handler_->HandleShowSyncSetupUI(nullptr);

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
  handler_->HandleShowSyncSetupUI(nullptr);

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(PeopleHandlerTest, TestSyncEverything) {
  SigninUser();
  CreatePeopleHandler();
  std::string args = GetConfiguration(nullptr, SYNC_ALL_DATA, GetAllTypes(),
                                      std::string(), ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(true, _));
  handler_->HandleSetDatatypes(&list_args);

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, TestPassphraseStillRequired) {
  SigninUser();
  CreatePeopleHandler();
  std::string args = GetConfiguration(nullptr, SYNC_ALL_DATA, GetAllTypes(),
                                      std::string(), ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  handler_->HandleSetEncryption(&list_args);
  // We should navigate back to the configure page since we need a passphrase.
  ExpectPageStatusResponse(PeopleHandler::kPassphraseFailedPageStatus);
}

TEST_F(PeopleHandlerTest, EnterExistingFrozenImplicitPassword) {
  SigninUser();
  CreatePeopleHandler();
  base::DictionaryValue dict;
  dict.SetBoolean("setNewPassphrase", false);
  std::string args = GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(),
                                      "oldGaiaPassphrase", ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  // Act as if an encryption passphrase is required the first time, then never
  // again after that.
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              IsPassphraseRequired())
      .WillOnce(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase("oldGaiaPassphrase"))
      .WillOnce(Return(true));

  handler_->HandleSetEncryption(&list_args);
  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, SetNewCustomPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  base::DictionaryValue dict;
  dict.SetBoolean("setNewPassphrase", true);
  std::string args = GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(),
                                      "custom_passphrase", ENCRYPT_ALL_DATA);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsEncryptEverythingAllowed())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase("custom_passphrase"));

  handler_->HandleSetEncryption(&list_args);
  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, EnterWrongExistingPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  base::DictionaryValue dict;
  dict.SetBoolean("setNewPassphrase", false);
  std::string args = GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(),
                                      "invalid_passphrase", ENCRYPT_ALL_DATA);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase("invalid_passphrase"))
      .WillOnce(Return(false));

  SetDefaultExpectationsForConfigPage();

  handler_->HandleSetEncryption(&list_args);
  // We should navigate back to the configure page since we need a passphrase.
  ExpectPageStatusResponse(PeopleHandler::kPassphraseFailedPageStatus);
}

TEST_F(PeopleHandlerTest, EnterBlankExistingPassphrase) {
  SigninUser();
  CreatePeopleHandler();
  base::DictionaryValue dict;
  dict.SetBoolean("setNewPassphrase", false);
  std::string args = GetConfiguration(&dict,
                                      SYNC_ALL_DATA,
                                      GetAllTypes(),
                                      "",
                                      ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();

  SetDefaultExpectationsForConfigPage();

  handler_->HandleSetEncryption(&list_args);
  // We should navigate back to the configure page since we need a passphrase.
  ExpectPageStatusResponse(PeopleHandler::kPassphraseFailedPageStatus);
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
    std::string args =
        GetConfiguration(nullptr, CHOOSE_WHAT_TO_SYNC, type_to_set,
                         std::string(), ENCRYPT_PASSWORDS);
    base::ListValue list_args;
    list_args.AppendString(kTestCallbackId);
    list_args.AppendString(args);
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsPassphraseRequiredForPreferredDataTypes())
        .WillByDefault(Return(false));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(false));
    SetupInitializedSyncService();
    EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
                SetSelectedTypes(false, type_to_set));

    handler_->HandleSetDatatypes(&list_args);
    ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
  }
}

TEST_F(PeopleHandlerTest, TestSyncAllManually) {
  SigninUser();
  CreatePeopleHandler();
  SetDefaultExpectationsForConfigPage();
  std::string args =
      GetConfiguration(nullptr, CHOOSE_WHAT_TO_SYNC, GetAllTypes(),
                       std::string(), ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(false, GetAllTypes()));
  handler_->HandleSetDatatypes(&list_args);

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
  std::string config =
      GetConfiguration(/*extra_values=*/nullptr, CHOOSE_WHAT_TO_SYNC,
                       GetAllTypes(), std::string(), ENCRYPT_PASSWORDS);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(config);

  // Only the registered types are selected.
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSelectedTypes(/*sync_everything=*/false, registered_types));
  handler_->HandleSetDatatypes(&list_args);
}

TEST_F(PeopleHandlerTest, ShowSyncSetup) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  // This should display the sync setup dialog (not login).
  SetDefaultExpectationsForConfigPage();
  handler_->HandleShowSyncSetupUI(nullptr);

  ExpectSyncPrefsChanged();
}

TEST_F(PeopleHandlerTest, ShowSetupSyncEverything) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "appsRegistered", true);
  CheckBool(dictionary, "autofillRegistered", true);
  CheckBool(dictionary, "bookmarksRegistered", true);
  CheckBool(dictionary, "extensionsRegistered", true);
  CheckBool(dictionary, "passwordsRegistered", true);
  CheckBool(dictionary, "preferencesRegistered", true);
  CheckBool(dictionary, "tabsRegistered", true);
  CheckBool(dictionary, "themesRegistered", true);
  CheckBool(dictionary, "typedUrlsRegistered", true);
  CheckBool(dictionary, "paymentsIntegrationEnabled", true);
  CheckBool(dictionary, "passphraseRequired", false);
  CheckBool(dictionary, "encryptAllData", false);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_DATA, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupManuallySyncAll) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncEverythingEnabled())
      .WillByDefault(Return(false));
  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  SigninUser();
  CreatePeopleHandler();
  for (syncer::UserSelectableType type : GetAllTypes()) {
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(false));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsUsingSecondaryPassphrase())
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
    handler_->HandleShowSyncSetupUI(nullptr);

    // Close the config overlay.
    LoginUIServiceFactory::GetForProfile(profile())->LoginUIClosed(
        handler_.get());

    const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
    // Clean up so we can loop back to display the dialog again.
    web_ui_.ClearTrackedCalls();
  }
}

TEST_F(PeopleHandlerTest, ShowSetupOldGaiaPassphraseRequired) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kFrozenImplicitPassphrase));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "passphraseRequired", true);
  EXPECT_TRUE(dictionary->FindKey("enterPassphraseBody"));
}

TEST_F(PeopleHandlerTest, ShowSetupCustomPassphraseRequired) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kCustomPassphrase));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "passphraseRequired", true);
  EXPECT_TRUE(dictionary->FindKey("enterPassphraseBody"));
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
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "passphraseRequired", false);
  CheckBool(dictionary, "trustedVaultKeysRequired", true);
  EXPECT_FALSE(dictionary->FindKey("enterPassphraseBody"));
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAll) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsEncryptEverythingEnabled())
      .WillByDefault(Return(true));

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "encryptAllData", true);
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAllDisallowed) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  SetDefaultExpectationsForConfigPage();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsEncryptEverythingAllowed())
      .WillByDefault(Return(false));

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(nullptr);

  const base::DictionaryValue* dictionary = ExpectSyncPrefsChanged();
  CheckBool(dictionary, "encryptAllData", false);
  CheckBool(dictionary, "encryptAllDataAllowed", false);
}

TEST_F(PeopleHandlerTest, TurnOnEncryptAllDisallowed) {
  SigninUser();
  CreatePeopleHandler();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsPassphraseRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  SetupInitializedSyncService();
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsEncryptEverythingAllowed())
      .WillByDefault(Return(false));

  base::DictionaryValue dict;
  dict.SetBoolean("setNewPassphrase", true);
  std::string args = GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(),
                                      "password", ENCRYPT_ALL_DATA);
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(args);

  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              EnableEncryptEverything())
      .Times(0);
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase(_))
      .Times(0);

  handler_->HandleSetEncryption(&list_args);

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmSoon) {
  SigninUser();
  CreatePeopleHandler();
  // Sync starts out fully enabled.
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(nullptr);

  // Now sync gets reset from the dashboard (the user clicked the "Manage synced
  // data" link), which results in the sync-requested and first-setup-complete
  // bits being cleared.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
      .WillByDefault(Return(false));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));
  // Sync will eventually start again in transport mode.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));

  NotifySyncStateChanged();

  // Now the user confirms sync again. This should set both the sync-requested
  // and the first-setup-complete bits.
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true))
      .WillOnce([&](bool) {
        // SetSyncRequested(true) clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
            .WillByDefault(Return(true));
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

  base::ListValue did_abort;
  did_abort.Append(base::Value(false));
  handler_->OnDidClosePage(&did_abort);
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmLater) {
  SigninUser();
  CreatePeopleHandler();
  // Sync starts out fully enabled.
  SetDefaultExpectationsForConfigPage();

  handler_->HandleShowSyncSetupUI(nullptr);

  // Now sync gets reset from the dashboard (the user clicked the "Manage synced
  // data" link), which results in the sync-requested and first-setup-complete
  // bits being cleared.
  ON_CALL(*mock_sync_service_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
      .WillByDefault(Return(false));
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
  EXPECT_CALL(*mock_sync_service_->GetMockUserSettings(),
              SetSyncRequested(true))
      .WillOnce([&](bool) {
        // SetSyncRequested(true) clears DISABLE_REASON_USER_CHOICE, and
        // immediately starts initializing the engine.
        ON_CALL(*mock_sync_service_, GetDisableReasons())
            .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
        ON_CALL(*mock_sync_service_->GetMockUserSettings(), IsSyncRequested())
            .WillByDefault(Return(true));
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

  base::ListValue did_abort;
  did_abort.Append(base::Value(false));
  handler_->OnDidClosePage(&did_abort);
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
  identity_test_env->SetPrimaryAccount(account_1.email);

  PeopleHandler handler(profile.get());
  base::Value accounts = handler.GetStoredAccountsList();

  ASSERT_TRUE(accounts.is_list());
  base::Value::ConstListView accounts_list = accounts.GetList();

  ASSERT_EQ(2u, accounts_list.size());
  ASSERT_TRUE(accounts_list[0].FindKey("email"));
  ASSERT_TRUE(accounts_list[1].FindKey("email"));
  EXPECT_EQ("a@gmail.com", accounts_list[0].FindKey("email")->GetString());
  EXPECT_EQ("b@gmail.com", accounts_list[1].FindKey("email")->GetString());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if defined(OS_CHROMEOS)
// Regression test for crash in guest mode. https://crbug.com/1040476
TEST(PeopleHandlerGuestModeTest, GetStoredAccountsList) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<Profile> profile = builder.Build();

  PeopleHandler handler(profile.get());
  base::Value accounts = handler.GetStoredAccountsList();
  EXPECT_TRUE(accounts.GetList().empty());
}

TEST_F(PeopleHandlerTest, TurnOffSync) {
  // Simulate a user who previously turned on sync.
  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com");
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  CreatePeopleHandler();
  handler_->HandleTurnOffSync(nullptr);
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  const base::DictionaryValue* status = ExpectSyncStatusChanged();
  CheckBool(status, "signedIn", false);
}

TEST_F(PeopleHandlerTest, GetStoredAccountsList) {
  // Chrome OS sets an unconsented primary account on login.
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  CreatePeopleHandler();
  base::Value accounts = handler_->GetStoredAccountsList();
  base::Value::ListView accounts_list = accounts.GetList();
  ASSERT_EQ(1u, accounts_list.size());
  EXPECT_EQ("user@gmail.com", accounts_list[0].FindKey("email")->GetString());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
