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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
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
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using signin::ConsentLevel;
using signin_util::SignedInState;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Const;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Values;

constexpr char kTestUser[] = "chrome_p13n_test@gmail.com";
constexpr char kTestCallbackId[] = "test-callback-id";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Event fired when calling
// `PeopleHandler::UpdateChromeSigninUserChoiceInfo()`.
constexpr char kChromeSigninUserChoiceInfoChangeEventName[] =
    "chrome-signin-user-choice-info-change";

// Event fired when calling `PeopleHandler::UpdateSyncStatus()`.
constexpr char kSyncStatusChangeEventName[] = "sync-status-changed";
#endif

// Returns a UserSelectableTypeSet with all types set.
syncer::UserSelectableTypeSet GetAllTypes() {
  return syncer::UserSelectableTypeSet::All();
}

enum SyncAllDataConfig { SYNC_ALL_DATA, CHOOSE_WHAT_TO_SYNC };

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
  result.Set("cookiesSynced", types.Has(syncer::UserSelectableType::kCookies));
  result.Set("extensionsSynced",
             types.Has(syncer::UserSelectableType::kExtensions));
  result.Set("passwordsSynced",
             types.Has(syncer::UserSelectableType::kPasswords));
  result.Set("paymentsSynced",
             types.Has(syncer::UserSelectableType::kPayments));
  result.Set("preferencesSynced",
             types.Has(syncer::UserSelectableType::kPreferences));
  result.Set("productComparisonSynced",
             types.Has(syncer::UserSelectableType::kProductComparison));
  result.Set("readingListSynced",
             types.Has(syncer::UserSelectableType::kReadingList));
  result.Set("savedTabGroupsSynced",
             types.Has(syncer::UserSelectableType::kSavedTabGroups));
  result.Set("sharedTabGroupDataSynced",
             types.Has(syncer::UserSelectableType::kSharedTabGroupData));
  result.Set("tabsSynced", types.Has(syncer::UserSelectableType::kTabs));
  result.Set("themesSynced", types.Has(syncer::UserSelectableType::kThemes));
  result.Set("typedUrlsSynced",
             types.Has(syncer::UserSelectableType::kHistory));

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
  ExpectHasBoolKey(dictionary, "cookiesSynced",
                   types.Has(syncer::UserSelectableType::kCookies));
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
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
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

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));

    sync_service_->SetSignedOut();
  }

  void TearDown() override {
    sync_service_ = nullptr;
    DestroyPeopleHandler();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SigninUserWithoutSyncFeature() {
    const CoreAccountInfo account_info = identity_test_env()->SetPrimaryAccount(
        kTestUser, signin::ConsentLevel::kSignin);
    sync_service_->SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  }

  void SigninUserAndTurnSyncFeatureOn() {
    const CoreAccountInfo account_info = identity_test_env()->SetPrimaryAccount(
        kTestUser, signin::ConsentLevel::kSync);
    sync_service_->SetSignedIn(signin::ConsentLevel::kSync, account_info);
  }

  void CreatePeopleHandler() {
    handler_ = std::make_unique<TestingPeopleHandler>(&web_ui_, profile());
    handler_->AllowJavascript();
    web_ui_.set_web_contents(web_contents());
  }

  void DestroyPeopleHandler() {
    if (handler_) {
      handler_->set_web_ui(nullptr);
      handler_->DisallowJavascript();
      handler_ = nullptr;
    }
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

  // Returns all fired sync-prefs-changed events, without any validation.
  std::vector<const base::Value*> GetFiredSyncPrefsChanged() {
    return GetAllFiredValuesForEventName("sync-prefs-changed");
  }

  // Must be called at most once per test to check if a sync-prefs-changed
  // event happened. Returns the single fired value.
  base::Value::Dict ExpectSyncPrefsChanged() {
    std::vector<const base::Value*> args = GetFiredSyncPrefsChanged();
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

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  syncer::TestSyncUserSettings* sync_user_settings() {
    return sync_service_->GetUserSettings();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::TestSyncService> sync_service_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingPeopleHandler> handler_;
  base::test::ScopedFeatureList feature_list_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PeopleHandlerTest, DisplayBasicLogin) {
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      ConsentLevel::kSignin));
  CreatePeopleHandler();
  // Test that the HandleStartSignin call enables JavaScript.
  handler_->DisallowJavascript();

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
  SigninUserAndTurnSyncFeatureOn();
  sync_user_settings()->ClearInitialSyncFeatureSetupComplete();

  CreatePeopleHandler();

  ASSERT_THAT(sync_service_->GetDisableReasons(), IsEmpty());

  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

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
  SigninUserAndTurnSyncFeatureOn();
  sync_user_settings()->ClearInitialSyncFeatureSetupComplete();

  CreatePeopleHandler();

  ASSERT_THAT(sync_service_->GetDisableReasons(), IsEmpty());

  // Sync engine is stopped initially, and will start up.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);

  handler_->HandleShowSyncSetupUI(base::Value::List());

  // Mimic engine initialization.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  sync_service_->FireStateChanged();

  // No pref updates sent yet, because the engine is not initialized.
  EXPECT_EQ(0U, GetFiredSyncPrefsChanged().size());
  web_ui_.ClearTrackedCalls();

  // Now, act as if the SyncService has started up.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_->FireStateChanged();

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
// initialized. This isn't reachable on Ash because
// IsInitialSyncFeatureSetupComplete() always returns true.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PeopleHandlerTest,
       DisplayConfigureWithEngineDisabledAndCancelAfterSigninSuccess) {
  SigninUserAndTurnSyncFeatureOn();
  sync_user_settings()->ClearInitialSyncFeatureSetupComplete();

  CreatePeopleHandler();

  handler_->HandleShowSyncSetupUI(base::Value::List());

  EXPECT_TRUE(sync_service_->IsSetupInProgress());

  // Sync engine becomes active, so |handler_| is notified.
  ASSERT_EQ(sync_service_->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  handler_->CloseSyncSetup();
  EXPECT_EQ(
      nullptr,
      LoginUIServiceFactory::GetForProfile(profile())->current_login_ui());

  EXPECT_FALSE(sync_service_->IsSetupInProgress());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PeopleHandlerTest, RestartSyncAfterDashboardClear) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  // Mimic a dashboard clear, which should reset the sync engine and restart it
  // in transport mode. Defer the second initialization of the engine, to test
  // that prefs are not sent yet.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  sync_service_->MimicDashboardClear();

  ASSERT_EQ(sync_service_->GetTransportState(),
            syncer::SyncService::TransportState::INITIALIZING);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(sync_user_settings()->IsSyncFeatureDisabledViaDashboard());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  handler_->HandleShowSyncSetupUI(base::Value::List());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(sync_user_settings()->IsSyncFeatureDisabledViaDashboard());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Since the engine is not initialized yet, no prefs should be sent.
  EXPECT_EQ(0U, GetFiredSyncPrefsChanged().size());

  // Now, allow the sync engine to fully start.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_->FireStateChanged();

  // Upon initialization of the engine, the new prefs should be sent.
  ExpectSyncPrefsChanged();
}

// Tests that signals not related to user intention to configure sync don't
// trigger sync engine start.
TEST_F(PeopleHandlerTest, OnlyStartEngineWhenConfiguringSync) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  sync_service_->FireStateChanged();
  EXPECT_EQ(sync_service_->GetTransportState(),
            syncer::SyncService::TransportState::START_DEFERRED);
}

TEST_F(PeopleHandlerTest, AcquireSyncBlockerWhenLoadingSyncSettingsSubpage) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  // Remove the WebUIConfig for chrome::kSyncSetupSubPage to prevent a new web
  // ui from being created when we navigate to a page that would normally create
  // one.
  content::ScopedWebUIConfigRegistration registration(
      chrome::GetSettingsUrl(chrome::kSyncSetupSubPage));

  EXPECT_FALSE(handler_->sync_blocker_);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      chrome::GetSettingsUrl(chrome::kSyncSetupSubPage), web_contents());
  navigation->Start();
  handler_->InitializeSyncBlocker();

  EXPECT_TRUE(handler_->sync_blocker_);
}

TEST_F(PeopleHandlerTest, UnrecoverableErrorInitializingSync) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_service_->SetHasUnrecoverableError(true);
  sync_user_settings()->ClearInitialSyncFeatureSetupComplete();

  // Open the web UI.
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(PeopleHandlerTest, GaiaErrorInitializingSync) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_service_->SetSignedOut();

  // Open the web UI.
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(PeopleHandlerTest, TestSyncEverything) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetSelectedTypes(/*sync_everything=*/false,
                                         /*types=*/GetAllTypes());

  std::string args = GetConfiguration(SYNC_ALL_DATA, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(args);
  handler_->HandleSetDatatypes(list_args);
  EXPECT_TRUE(sync_user_settings()->IsSyncEverythingEnabled());

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);
}

TEST_F(PeopleHandlerTest, EnterCorrectExistingPassphrase) {
  const std::string kCorrectPassphrase = "correct_passphrase";

  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetPassphraseRequired(kCorrectPassphrase);

  ASSERT_TRUE(sync_user_settings()->IsPassphraseRequired());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(kCorrectPassphrase);
  handler_->HandleSetDecryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(true);

  EXPECT_FALSE(sync_user_settings()->IsPassphraseRequired());
}

TEST_F(PeopleHandlerTest, SuccessfullyCreateCustomPassphrase) {
  const std::string kPassphrase = "custom_passphrase";

  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  ASSERT_FALSE(sync_user_settings()->IsUsingExplicitPassphrase());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(kPassphrase);
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(true);

  EXPECT_EQ(sync_user_settings()->GetEncryptionPassphrase(), kPassphrase);
}

TEST_F(PeopleHandlerTest, EnterWrongExistingPassphrase) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetPassphraseRequired("correct_passphrase");

  ASSERT_TRUE(sync_user_settings()->IsPassphraseRequired());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("invalid_passphrase");
  handler_->HandleSetDecryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);

  ASSERT_TRUE(sync_user_settings()->IsPassphraseRequired());
}

TEST_F(PeopleHandlerTest, CannotCreateBlankPassphrase) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  ASSERT_FALSE(sync_user_settings()->IsUsingExplicitPassphrase());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);

  ASSERT_FALSE(sync_user_settings()->IsUsingExplicitPassphrase());
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(PeopleHandlerTest, TestSyncIndividualTypes) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  for (syncer::UserSelectableType type : GetAllTypes()) {
    syncer::UserSelectableTypeSet type_to_set;
    type_to_set.Put(type);

    std::string args = GetConfiguration(CHOOSE_WHAT_TO_SYNC, type_to_set);
    base::Value::List list_args;
    list_args.Append(kTestCallbackId);
    list_args.Append(args);

    handler_->HandleSetDatatypes(list_args);
    ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);

    EXPECT_FALSE(sync_user_settings()->IsSyncEverythingEnabled());
    EXPECT_EQ(sync_user_settings()->GetSelectedTypes(), type_to_set);
  }
}

TEST_F(PeopleHandlerTest, TestSyncAllManually) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  std::string args = GetConfiguration(CHOOSE_WHAT_TO_SYNC, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(args);
  handler_->HandleSetDatatypes(list_args);

  ExpectPageStatusResponse(PeopleHandler::kConfigurePageStatus);

  EXPECT_FALSE(sync_user_settings()->IsSyncEverythingEnabled());
  EXPECT_EQ(sync_user_settings()->GetSelectedTypes(), GetAllTypes());
}

TEST_F(PeopleHandlerTest, NonRegisteredType) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  // Simulate apps not being registered.
  syncer::UserSelectableTypeSet registered_types = GetAllTypes();
  registered_types.Remove(syncer::UserSelectableType::kApps);
  sync_user_settings()->SetRegisteredSelectableTypes(registered_types);

  // Simulate "Sync everything" being turned off, but all individual
  // toggles left on.
  std::string config = GetConfiguration(CHOOSE_WHAT_TO_SYNC, GetAllTypes());
  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append(config);

  // Only the registered types are selected.
  handler_->HandleSetDatatypes(list_args);
  EXPECT_FALSE(sync_user_settings()->IsSyncEverythingEnabled());
  EXPECT_EQ(sync_user_settings()->GetSelectedTypes(), registered_types);
}

TEST_F(PeopleHandlerTest, ShowSyncSetup) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  ExpectSyncPrefsChanged();
}

TEST_F(PeopleHandlerTest, ShowSetupSyncEverything) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "syncAllDataTypes", true);
  ExpectHasBoolKey(dictionary, "appsRegistered", true);
  ExpectHasBoolKey(dictionary, "autofillRegistered", true);
  ExpectHasBoolKey(dictionary, "bookmarksRegistered", true);
  ExpectHasBoolKey(dictionary, "cookiesRegistered", true);
  ExpectHasBoolKey(dictionary, "extensionsRegistered", true);
  ExpectHasBoolKey(dictionary, "passwordsRegistered", true);
  ExpectHasBoolKey(dictionary, "paymentsRegistered", true);
  ExpectHasBoolKey(dictionary, "preferencesRegistered", true);
  ExpectHasBoolKey(dictionary, "readingListRegistered", true);
  ExpectHasBoolKey(dictionary, "tabsRegistered", true);
  ExpectHasBoolKey(dictionary, "themesRegistered", true);
  ExpectHasBoolKey(dictionary, "typedUrlsRegistered", true);
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
  ExpectHasBoolKey(dictionary, "encryptAllData", false);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_DATA, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupManuallySyncAll) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_user_settings()->SetSelectedTypes(/*sync_everything=*/false,
                                         /*types=*/GetAllTypes());
  ASSERT_FALSE(sync_user_settings()->IsSyncEverythingEnabled());

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, GetAllTypes());
}

TEST_F(PeopleHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  for (syncer::UserSelectableType type : GetAllTypes()) {
    const syncer::UserSelectableTypeSet types = {type};

    sync_user_settings()->SetSelectedTypes(/*sync_everything=*/false, types);

    // This should display the sync setup dialog (not login).
    handler_->HandleShowSyncSetupUI(base::Value::List());

    // Close the config overlay.
    LoginUIServiceFactory::GetForProfile(profile())->LoginUIClosed(
        handler_.get());

    base::Value::Dict dictionary = ExpectSyncPrefsChanged();
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types);

    // Clean up so we can loop back to display the dialog again.
    web_ui_.ClearTrackedCalls();
  }
}

TEST_F(PeopleHandlerTest, ShowSetupOldGaiaPassphraseRequired) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  const auto passphrase_time = base::Time::Now();

  sync_user_settings()->SetPassphraseRequired();
  sync_user_settings()->SetPassphraseType(
      syncer::PassphraseType::kFrozenImplicitPassphrase);
  sync_user_settings()->SetExplicitPassphraseTime(passphrase_time);

  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", true);
  ASSERT_TRUE(dictionary.contains("explicitPassphraseTime"));
  ASSERT_TRUE(dictionary.FindString("explicitPassphraseTime"));
  EXPECT_EQ(base::UTF16ToUTF8(base::TimeFormatShortDate(passphrase_time)),
            *dictionary.FindString("explicitPassphraseTime"));
}

TEST_F(PeopleHandlerTest, ShowSetupCustomPassphraseRequired) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  const auto passphrase_time = base::Time::Now();

  sync_user_settings()->SetPassphraseRequired();
  sync_user_settings()->SetPassphraseType(
      syncer::PassphraseType::kCustomPassphrase);
  sync_user_settings()->SetExplicitPassphraseTime(passphrase_time);

  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", true);
  ASSERT_TRUE(dictionary.contains("explicitPassphraseTime"));
  ASSERT_TRUE(dictionary.FindString("explicitPassphraseTime"));
  EXPECT_EQ(base::UTF16ToUTF8(base::TimeFormatShortDate(passphrase_time)),
            *dictionary.FindString("explicitPassphraseTime"));
}

// Verifies that the user is not prompted to enter the custom passphrase while
// sync setup is ongoing. This isn't reachable on Ash because
// IsInitialSyncFeatureSetupComplete() always returns true.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PeopleHandlerTest, OngoingSetupCustomPassphraseRequired) {
  SigninUserWithoutSyncFeature();
  CreatePeopleHandler();

  const auto passphrase_time = base::Time::Now();

  sync_service_->SetSyncFeatureRequested();
  sync_user_settings()->SetPassphraseRequired();
  sync_user_settings()->SetPassphraseType(
      syncer::PassphraseType::kCustomPassphrase);
  sync_user_settings()->SetExplicitPassphraseTime(passphrase_time);

  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PeopleHandlerTest, ShowSetupTrustedVaultKeysRequired) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_->SetTrustedVaultKeyRequired(true);

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "passphraseRequired", false);
  ExpectHasBoolKey(dictionary, "trustedVaultKeysRequired", true);
  EXPECT_FALSE(dictionary.contains("explicitPassphraseTime"));
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAll) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_user_settings()->SetIsUsingExplicitPassphrase(true);
  ASSERT_TRUE(sync_user_settings()->IsEncryptEverythingEnabled());

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "encryptAllData", true);
}

TEST_F(PeopleHandlerTest, ShowSetupEncryptAllDisallowed) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();
  sync_user_settings()->SetCustomPassphraseAllowed(false);

  // This should display the sync setup dialog (not login).
  handler_->HandleShowSyncSetupUI(base::Value::List());

  base::Value::Dict dictionary = ExpectSyncPrefsChanged();
  ExpectHasBoolKey(dictionary, "encryptAllData", false);
  ExpectHasBoolKey(dictionary, "customPassphraseAllowed", false);
}

TEST_F(PeopleHandlerTest, CannotCreatePassphraseIfCustomPassphraseDisallowed) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetCustomPassphraseAllowed(false);

  ASSERT_FALSE(sync_user_settings()->IsUsingExplicitPassphrase());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("passphrase123");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);

  EXPECT_FALSE(sync_user_settings()->IsUsingExplicitPassphrase());
}

TEST_F(PeopleHandlerTest, CannotOverwritePassphraseWithNewOne) {
  const std::string kInitialPassphrase = "initial_passphrase";

  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  sync_user_settings()->SetEncryptionPassphrase(kInitialPassphrase);
  ASSERT_TRUE(sync_user_settings()->IsUsingExplicitPassphrase());

  base::Value::List list_args;
  list_args.Append(kTestCallbackId);
  list_args.Append("passphrase123");
  handler_->HandleSetEncryptionPassphrase(list_args);

  ExpectSetPassphraseSuccess(false);

  EXPECT_EQ(sync_user_settings()->GetEncryptionPassphrase(),
            kInitialPassphrase);
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmSoon) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  ASSERT_TRUE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());

  handler_->HandleShowSyncSetupUI(base::Value::List());

  sync_service_->MimicDashboardClear();
  sync_service_->FireStateChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(sync_user_settings()->IsSyncFeatureDisabledViaDashboard());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Now the user confirms sync again. This should set both the sync-requested
  // and the first-setup-complete bits.
  base::Value::List did_abort;
  did_abort.Append(false);
  handler_->OnDidClosePage(did_abort);

  EXPECT_TRUE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());
}

TEST_F(PeopleHandlerTest, DashboardClearWhileSettingsOpen_ConfirmLater) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  ASSERT_TRUE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());

  handler_->HandleShowSyncSetupUI(base::Value::List());

  sync_service_->MimicDashboardClear();
  sync_service_->FireStateChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(sync_user_settings()->IsSyncFeatureDisabledViaDashboard());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Sync starts up in transport mode.
  ASSERT_EQ(sync_service_->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  // Now the user confirms sync again. This should set the sync-requested bit
  // and also the first-setup-complete bit (except on ChromeOS Ash where it is
  // always true).
  base::Value::List did_abort;
  did_abort.Append(false);
  handler_->OnDidClosePage(did_abort);

  EXPECT_EQ(sync_service_->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(sync_user_settings()->IsInitialSyncFeatureSetupComplete());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(PeopleHandlerDiceTest, StoredAccountsList) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  content::BrowserTaskEnvironment task_environment;

  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  ASSERT_EQ(true, AccountConsistencyModeManager::IsDiceEnabledForProfile(
                      profile.get()));

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  auto account_1 = identity_test_env->MakeAccountAvailable(
      "a@gmail.com", {.set_cookie = true});
  auto account_2 = identity_test_env->MakeAccountAvailable(
      "b@gmail.com", {.set_cookie = true});
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
          CreateProfileForIdentityTestEnvironment(builder);

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
          CreateProfileForIdentityTestEnvironment(builder);

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
          CreateProfileForIdentityTestEnvironment(builder);

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

  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(builder);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);
  auto* identity_manager = identity_test_env->identity_manager();

  identity_test_env->MakePrimaryAccountAvailable("user@gmail.com",
                                                 ConsentLevel::kSignin);
  ASSERT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));

  identity_test_env->MakeAccountAvailable("a@gmail.com", {.set_cookie = true});
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

  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(false);
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(builder);

  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);
  auto* identity_manager = identity_test_env->identity_manager();

  auto account_1 = identity_test_env->MakeAccountAvailable(
      "a@gmail.com", {.set_cookie = true});
  auto account_2 = identity_test_env->MakeAccountAvailable(
      "b@gmail.com", {.set_cookie = true});
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

TEST_F(PeopleHandlerTest, SyncCookiesDisabled) {
  base::test::ScopedFeatureList features;
  // Disable Floating SSO feature flag.
  features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFloatingSso});

  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  const base::Value::Dict& sync_status_values =
      handler_->GetSyncStatusDictionary();
  std::optional<bool> sync_cookies_supported =
      sync_status_values.FindBool("syncCookiesSupported");
  EXPECT_FALSE(sync_cookies_supported.has_value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class PeopleHandlerWithExplicitBrowserSigninTest : public PeopleHandlerTest {
 public:
  // Checks values returned as a response of WebUI called.
  void ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      bool should_show_settings,
      ChromeSigninUserChoice expected_choice,
      const std::string& expected_signed_in_email) {
    auto& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool())
        << "Callback should be resolved with a boolean indicating the success, "
           "never rejected.";
    ASSERT_TRUE(data.arg3()->is_dict());

    const base::Value::Dict& dict = data.arg3()->GetDict();
    ExpectChromeSigninUserChoiceInfoDict(
        dict, should_show_settings, expected_choice, expected_signed_in_email);
  }

  // Checks values returned from firing the change event.
  // Reads the last event. Expecting at least one event.
  void ExpectChromeSigninUserChoiceInfoFromLastChangeEvent(
      bool should_show_settings,
      ChromeSigninUserChoice expected_choice,
      const std::string& expected_signed_in_email) {
    auto values_list = GetAllFiredValuesForEventName(
        kChromeSigninUserChoiceInfoChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]);
    ASSERT_TRUE(values_list[last_index]->is_dict());

    const base::Value::Dict& values_dict = values_list[last_index]->GetDict();
    ExpectChromeSigninUserChoiceInfoDict(values_dict, should_show_settings,
                                         expected_choice,
                                         expected_signed_in_email);
  }

  // Tests the Dict content returned for the WebUI for
  // ChromeSigninUserChoiceInfo.
  static void ExpectChromeSigninUserChoiceInfoDict(
      const base::Value::Dict& values_dict,
      bool expected_should_show_settings,
      ChromeSigninUserChoice expected_choice,
      const std::string& expected_signed_in_email) {
    std::optional<bool> should_show_settings =
        values_dict.FindBool("shouldShowSettings");
    ASSERT_TRUE(should_show_settings.has_value());
    EXPECT_EQ(should_show_settings.value(), expected_should_show_settings);

    std::optional<int> choice_int = values_dict.FindInt("choice");
    ASSERT_TRUE(choice_int.has_value());
    EXPECT_EQ(static_cast<ChromeSigninUserChoice>(choice_int.value()),
              expected_choice);

    const std::string* signed_in_email =
        values_dict.FindString("signedInEmail");
    ASSERT_TRUE(signed_in_email);
    EXPECT_EQ(*signed_in_email, expected_signed_in_email);
  }

  bool HasChromeSigninUserChoiceInfoChangeEvent() {
    return !GetAllFiredValuesForEventName(
                kChromeSigninUserChoiceInfoChangeEventName)
                .empty();
  }

  void SetExplicitSignin(bool value) {
    profile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, value);
  }

  void TriggerPrimaryAccountInPersistentError() {
    ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

    // Inject the error.
    identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  }

  bool HasSyncStatusUpdateChangedEvent() {
    return !GetAllFiredValuesForEventName(kSyncStatusChangeEventName).empty();
  }

  void SimluateReauth() {
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    // Clear the error.
    identity_test_env()->SetRefreshTokenForPrimaryAccount();
  }

  void SimulateSignout() {
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    identity_test_env()->ClearPrimaryAccount();
  }

  void SimulateHandleGetChromeSigninUserChoiceInfo() const {
    base::Value::List args_get;
    args_get.Append(kTestCallbackId);
    handler_->HandleGetChromeSigninUserChoiceInfo(args_get);
  }

  void SimulateHandleSetChromeSigninUserChoiceInfo(
      std::string_view email,
      ChromeSigninUserChoice user_choice) {
    base::Value::List args_set;
    args_set.Append(static_cast<int>(user_choice));
    args_set.Append(email);
    handler_->HandleSetChromeSigninUserChoice(args_set);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest, ChromeSigninUserChoice) {
  base::HistogramTester histogram_tester;

  CreatePeopleHandler();

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      false, ChromeSigninUserChoice::kNoChoice, "");

  std::string email("user@gmail.com");
  identity_test_env()->MakePrimaryAccountAvailable(email,
                                                   ConsentLevel::kSignin);

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      true, ChromeSigninUserChoice::kNoChoice, email);

  ChromeSigninUserChoice user_choice = ChromeSigninUserChoice::kSignin;
  SimulateHandleSetChromeSigninUserChoiceInfo(email, user_choice);

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(true, user_choice, email);

  DestroyPeopleHandler();

  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSigninSettingModification", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.Settings.ChromeSigninSettingModification",
      /*`ChromeSigninSettingModification::kToSignin`*/ 2, 1);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserAvailableOnExplicitChromeSigninSignout) {
  const std::string email("user@gmail.com");
  identity_test_env()->MakePrimaryAccountAvailable(email,
                                                   ConsentLevel::kSignin);
  SetExplicitSignin(true);

  CreatePeopleHandler();

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      true, ChromeSigninUserChoice::kNoChoice, email);

  SimulateSignout();

  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      false, ChromeSigninUserChoice::kNoChoice, "");
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserAvailableOnDiceSignin) {
  const std::string email("user@gmail.com");
  identity_test_env()->MakePrimaryAccountAvailable(email,
                                                   ConsentLevel::kSignin);
  // Simulates Dice signin.
  SetExplicitSignin(false);

  CreatePeopleHandler();

  SimulateHandleGetChromeSigninUserChoiceInfo();
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      false, ChromeSigninUserChoice::kNoChoice, email);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserInfoUpdateOnPrefValueChange) {
  const std::string email("user@gmail.com");
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      email, ConsentLevel::kSignin);
  SetExplicitSignin(true);

  CreatePeopleHandler();

  ASSERT_FALSE(HasChromeSigninUserChoiceInfoChangeEvent());

  SigninPrefs signin_prefs(*profile()->GetPrefs());
  auto new_choice_value = ChromeSigninUserChoice::kSignin;
  ASSERT_NE(
      signin_prefs.GetChromeSigninInterceptionUserChoice(account_info.gaia),
      new_choice_value);
  signin_prefs.SetChromeSigninInterceptionUserChoice(account_info.gaia,
                                                     new_choice_value);
  ExpectChromeSigninUserChoiceInfoFromLastChangeEvent(true, new_choice_value,
                                                      email);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserInfoUpdateOnSignin) {
  CreatePeopleHandler();

  ASSERT_FALSE(HasChromeSigninUserChoiceInfoChangeEvent());

  const std::string email("user@gmail.com");
  // Explicit browser signin.
  identity_test_env()->MakePrimaryAccountAvailable(email,
                                                   ConsentLevel::kSignin);

  // By default no choice yet.
  ExpectChromeSigninUserChoiceInfoFromLastChangeEvent(
      true, ChromeSigninUserChoice::kNoChoice, email);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserInfoUpdateOnSync) {
  CreatePeopleHandler();

  ASSERT_FALSE(HasChromeSigninUserChoiceInfoChangeEvent());

  const std::string email("user@gmail.com");
  // Sync is a form of explicit browser signin.
  identity_test_env()->MakePrimaryAccountAvailable(email, ConsentLevel::kSync);

  // By default no choice yet.
  ExpectChromeSigninUserChoiceInfoFromLastChangeEvent(
      true, ChromeSigninUserChoice::kNoChoice, email);
}

// This test does not use `PeopleHandlerWithExplicitBrowserSigninTest` test
// suite and needs it's own test setup because in order to get the proper web
// signin, we need to set a cookie while signing in, which requires setting up a
// `TestURLLoaderFactory` with the `ChromeSigninClient`.
TEST(PeopleHandlerWebOnlySigninTest, ChromeSigninUserAvailableOnWebSignin) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kExplicitBrowserSigninUIOnDesktop};

  // -- Test Setup start

  // Needed to enable setting a proper account signed in on the web.
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  content::BrowserTaskEnvironment task_environment;

  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  // This test env should be used throughout the test.
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  PeopleHandler handler(profile.get());

  // -- Test Setup end.

  // Test before web signin -- only need to check the `shouldShowSettings` param
  {
    base::Value::Dict chrome_signin_user_choice_info_dict =
        handler.GetChromeSigninUserChoiceInfo();
    std::optional<bool> should_show_settings =
        chrome_signin_user_choice_info_dict.FindBool("shouldShowSettings");
    ASSERT_TRUE(should_show_settings.has_value());
    EXPECT_FALSE(should_show_settings.value());
  }

  const std::string email("user@gmail.com");
  // Signs in to the web only.
  identity_test_env->MakeAccountAvailable(email, {.set_cookie = true});
  ASSERT_FALSE(identity_test_env->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // Test after web signin and check all the fields.
  {
    base::Value::Dict chrome_signin_user_choice_info_dict =
        handler.GetChromeSigninUserChoiceInfo();
    PeopleHandlerWithExplicitBrowserSigninTest::
        ExpectChromeSigninUserChoiceInfoDict(
            chrome_signin_user_choice_info_dict,
            /*expected_should_show_settings=*/true,
            ChromeSigninUserChoice::kNoChoice, email);
  }

  // Check that `SignedInState` is properly computed
  {
    const base::Value::Dict& sync_status_values =
        handler.GetSyncStatusDictionary();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kWebOnlySignedIn);
  }
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest, SigninPendingThenSignout) {
  identity_test_env()->MakePrimaryAccountAvailable(kTestUser,
                                                   ConsentLevel::kSignin);
  SetExplicitSignin(true);
  CreatePeopleHandler();

  ASSERT_FALSE(HasSyncStatusUpdateChangedEvent());

  TriggerPrimaryAccountInPersistentError();

  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSignInPending);
  }

  // Simulates pressing on the "Sign out" Button in the Sign in Paused state,
  // that redirects to `PeopleHandler::HandleSignout()`. Since the test has no
  // browser, clearing the primary account should be equivalent.
  SimulateSignout();

  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSignedOut);
  }
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest, SigninPendingThenReauth) {
  identity_test_env()->MakePrimaryAccountAvailable(kTestUser,
                                                   ConsentLevel::kSignin);
  SetExplicitSignin(true);
  CreatePeopleHandler();

  ASSERT_FALSE(HasSyncStatusUpdateChangedEvent());

  TriggerPrimaryAccountInPersistentError();

  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSignInPending);
    ;
  }

  // Simulates pressing on the "Verify it's you" button in the Sign in Paused
  // state, and reauth.
  SimluateReauth();

  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSignedIn);
  }
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest, SigninPendingValueWithSync) {
  CreatePeopleHandler();

  ASSERT_FALSE(HasSyncStatusUpdateChangedEvent());

  // User is syncing.
  identity_test_env()->MakePrimaryAccountAvailable(kTestUser,
                                                   ConsentLevel::kSync);

  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSyncing);
  }

  // Invalidate the account while it is syncing.
  TriggerPrimaryAccountInPersistentError();

  // `SigninPending` is still false even when the account is in error.
  {
    auto values_list =
        GetAllFiredValuesForEventName(kSyncStatusChangeEventName);
    ASSERT_GT(values_list.size(), 0U);
    size_t last_index = values_list.size() - 1;
    ASSERT_TRUE(values_list[last_index]->is_dict());
    const base::Value::Dict& sync_status_values =
        values_list[last_index]->GetDict();
    std::optional<int> signedInState =
        sync_status_values.FindInt("signedInState");
    ASSERT_TRUE(signedInState.has_value());
    EXPECT_EQ(static_cast<SignedInState>(signedInState.value()),
              SignedInState::kSyncing);
  }
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserChoiceHistogramsWhenSignedOut) {
  base::HistogramTester histogram_tester;
  CreatePeopleHandler();

  // Simluates settings page loading.
  SimulateHandleGetChromeSigninUserChoiceInfo();

  // Simulates closing the settings page.
  DestroyPeopleHandler();

  // No account are signed in, the setting is not expected to be shown, so no
  // values related to it should be recorded.
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSigninSettingModification", 0);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserChoiceHistogramsWhenSignedInWithoutChangingSetting) {
  base::HistogramTester histogram_tester;
  // Signed in user can see the setting.
  identity_test_env()->MakePrimaryAccountAvailable("email@gmail.com",
                                                   ConsentLevel::kSignin);
  CreatePeopleHandler();

  // Simluates settings page loading.
  SimulateHandleGetChromeSigninUserChoiceInfo();

  // Simulates closing the settings page.
  DestroyPeopleHandler();

  // Setting is seen but not modiffied.
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSigninSettingModification", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.Settings.ChromeSigninSettingModification",
      /*`ChromeSigninSettingModification::kNoModification`*/ 0, 1);
}

TEST_F(PeopleHandlerWithExplicitBrowserSigninTest,
       ChromeSigninUserChoiceHistogramsWhenSignedInWithChangingSetting) {
  base::HistogramTester histogram_tester;
  // Signed in user can see the setting.
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      /*email=*/"email@gmail.com", ConsentLevel::kSignin);

  CreatePeopleHandler();

  // Simluates settings page loading.
  SimulateHandleGetChromeSigninUserChoiceInfo();

  SigninPrefs signin_prefs(*profile()->GetPrefs());
  ChromeSigninUserChoice current_choice =
      signin_prefs.GetChromeSigninInterceptionUserChoice(account.gaia);

  // Simulates setting a new value through the UI.
  ChromeSigninUserChoice user_choice = ChromeSigninUserChoice::kSignin;
  ASSERT_NE(current_choice, user_choice);
  SimulateHandleSetChromeSigninUserChoiceInfo(account.email, user_choice);

  // Simulate a last bubble decline time as well.
  signin_prefs.SetChromeSigninInterceptionLastBubbleDeclineTime(
      account.gaia, base::Time::Now());
  signin_prefs.IncrementChromeSigninBubbleRepromptCount(account.gaia);

  // Simulates a second selection within the same settings session.
  ChromeSigninUserChoice user_choice2 = ChromeSigninUserChoice::kDoNotSignin;
  ASSERT_NE(current_choice, user_choice2);
  SimulateHandleSetChromeSigninUserChoiceInfo(account.email, user_choice2);
  // Explicitly setting the do not sign in option should clear bubble declined
  // time.
  EXPECT_FALSE(
      signin_prefs
          .GetChromeSigninInterceptionLastBubbleDeclineTime(account.gaia)
          .has_value());
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(account.gaia), 0);

  // Enforcing changing the value to the same previous one should not record a
  // new modification.
  SimulateHandleSetChromeSigninUserChoiceInfo(account.email, user_choice2);

  // Simulates closing the settings page.
  DestroyPeopleHandler();

  // Setting is seen and modified twice.
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSigninSettingModification", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.Settings.ChromeSigninSettingModification",
      /*`ChromeSigninSettingModification::kToSignin`*/ 2, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.Settings.ChromeSigninSettingModification",
      /*`ChromeSigninSettingModification::kToDoNotSignin`*/ 3, 1);
}

TEST_F(
    PeopleHandlerWithExplicitBrowserSigninTest,
    ChromeSigninUserChoiceHistogramsWhenSignedInWithChangingSettingThenSignout) {
  base::HistogramTester histogram_tester;
  // Signed in user can see the setting.
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      /*email=*/"email@gmail.com", ConsentLevel::kSignin);

  CreatePeopleHandler();

  // Simluates settings page loading.
  SimulateHandleGetChromeSigninUserChoiceInfo();

  SigninPrefs signin_prefs(*profile()->GetPrefs());
  ChromeSigninUserChoice current_choice =
      signin_prefs.GetChromeSigninInterceptionUserChoice(account.gaia);

  // Simulates setting a new value through the settings UI.
  ChromeSigninUserChoice new_value = ChromeSigninUserChoice::kSignin;
  ASSERT_NE(current_choice, new_value);
  SimulateHandleSetChromeSigninUserChoiceInfo(account.email, new_value);

  SimulateSignout();

  SimulateHandleGetChromeSigninUserChoiceInfo();
  // The setting should not be seen anymore.
  ExpectChromeSigninUserChoiceInfoFromWebUiResponse(
      false, ChromeSigninUserChoice::kNoChoice, "");

  // Simulates closing the settings page.
  DestroyPeopleHandler();

  // A modification value is still recorded, even after signing out, since a
  // modification occurred during the session.
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSigninSettingModification", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.Settings.ChromeSigninSettingModification",
      /*`ChromeSigninSettingModification::kToSignin`*/ 2, 1);
}

#endif

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

  void SimulateSignout(const base::Value::List& args) {
    handler()->HandleSignout(args);
  }

  content::WebUI* web_ui() { return handler()->web_ui(); }

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
        GetIdentityTestEnvironmentFactories();
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
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  GetSigninSlient(profile())->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);

  CreatePeopleHandler();
  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  EXPECT_DEATH(SimulateSignout(args), ".*");
}

TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOff) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  GetSigninSlient(profile())->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  EXPECT_DEATH(SimulateSignout(args), ".*");
}
#endif  // DCHECK_IS_ON()

TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOn) {
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
  SimulateSignout(args);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  // Signout not triggered on dice platforms.
  EXPECT_EQ(web_contents()->GetVisibleURL().spec(), chrome::kChromeUINewTabURL);
  EXPECT_NE(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->service_logout_url());
}

TEST_F(PeopleHandlerSignoutTest, SignoutWithSyncOff) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  SimulateSignout(args);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(web_contents()->GetVisibleURL(),
            switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
                ? GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL())
                : GaiaUrls::GetInstance()->service_logout_url());
#else
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

TEST_F(PeopleHandlerSignoutTest, SignoutWithSyncOn) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  EXPECT_NE(web_ui(), nullptr);
  EXPECT_NE(nullptr, web_ui()->GetWebContents());

  EXPECT_TRUE(chrome::FindBrowserWithTab(web_ui()->GetWebContents()));

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  SimulateSignout(args);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(web_contents()->GetVisibleURL(),
            switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
                ? GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL())
                : GaiaUrls::GetInstance()->service_logout_url());
#else
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ExplicitBrowserSigninPeopleHandlerSignoutTest
    : public PeopleHandlerSignoutTest {
 private:
  base::test::ScopedFeatureList features_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(ExplicitBrowserSigninPeopleHandlerSignoutTest, Signout) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  base::Value::List args;
  args.Append(/*delete_profile=*/false);
  SimulateSignout(args);
  EXPECT_EQ(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL()));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PeopleHandlerWithCookiesSyncTest : public PeopleHandlerTest {
 private:
  // Enable Floating SSO feature flag.
  base::test::ScopedFeatureList features_{ash::features::kFloatingSso};
};

TEST_F(PeopleHandlerWithCookiesSyncTest, SyncCookiesSupported) {
  SigninUserAndTurnSyncFeatureOn();
  CreatePeopleHandler();

  // Feature flag enabled, policy unset.
  {
    const base::Value::Dict& sync_status_values =
        handler_->GetSyncStatusDictionary();
    std::optional<bool> sync_cookies_supported =
        sync_status_values.FindBool("syncCookiesSupported");
    ASSERT_TRUE(sync_cookies_supported.has_value());
    EXPECT_FALSE(sync_cookies_supported.value());
  }

  // Feature flag enabled, policy set to false.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kFloatingSsoEnabled, false);

    const base::Value::Dict& sync_status_values =
        handler_->GetSyncStatusDictionary();
    std::optional<bool> sync_cookies_supported =
        sync_status_values.FindBool("syncCookiesSupported");
    ASSERT_TRUE(sync_cookies_supported.has_value());
    EXPECT_FALSE(sync_cookies_supported.value());
  }

  // Feature flag enabled, policy set to true.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kFloatingSsoEnabled, true);

    const base::Value::Dict& sync_status_values =
        handler_->GetSyncStatusDictionary();
    std::optional<bool> sync_cookies_supported =
        sync_status_values.FindBool("syncCookiesSupported");
    ASSERT_TRUE(sync_cookies_supported.has_value());
    EXPECT_TRUE(sync_cookies_supported.value());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace settings
