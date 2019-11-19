// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_user_settings_mock.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;
using syncer::UserSelectableTypeSet;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;

namespace content {
class BrowserContext;
}

namespace {

enum FeatureConfig { FEATURE_ENABLED, FEATURE_DISABLED };
enum SyncAllConfig { SYNC_ALL_OS_TYPES, CHOOSE_WHAT_TO_SYNC };

// Creates a dictionary with the key/value pairs appropriate for a call to
// HandleSetOsSyncDatatypes().
DictionaryValue CreateOsSyncPrefs(FeatureConfig feature,
                                  SyncAllConfig sync_all,
                                  UserSelectableOsTypeSet types) {
  DictionaryValue result;
  result.SetBoolean("featureEnabled", feature == FEATURE_ENABLED);
  result.SetBoolean("syncAllOsTypes", sync_all == SYNC_ALL_OS_TYPES);
  // Add all of our data types.
  result.SetBoolean("osPreferencesSynced",
                    types.Has(UserSelectableOsType::kOsPreferences));
  result.SetBoolean("printersSynced",
                    types.Has(UserSelectableOsType::kPrinters));
  return result;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|.
void CheckBool(const DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value) {
  bool actual_value;
  EXPECT_TRUE(dictionary->GetBoolean(key, &actual_value))
      << "No value found for " << key;
  EXPECT_EQ(expected_value, actual_value) << "Mismatch found for " << key;
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the JS layer.
void CheckConfigDataTypeArguments(const DictionaryValue* dictionary,
                                  SyncAllConfig config,
                                  UserSelectableOsTypeSet types) {
  CheckBool(dictionary, "syncAllOsTypes", config == SYNC_ALL_OS_TYPES);
  CheckBool(dictionary, "osPreferencesSynced",
            types.Has(UserSelectableOsType::kOsPreferences));
  CheckBool(dictionary, "printersSynced",
            types.Has(UserSelectableOsType::kPrinters));
}

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
}

class TestWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    return std::make_unique<content::WebUIController>(web_ui);
  }
};

class OsSyncHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  OsSyncHandlerTest() = default;
  ~OsSyncHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Sign in the user.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_adaptor_->identity_test_env()->SetPrimaryAccount(
        "test@gmail.com");

    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSyncService)));

    // Configure the sync service as enabled and syncing.
    ON_CALL(*mock_sync_service_, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*mock_sync_service_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*mock_sync_service_, IsAuthenticatedAccountPrimary())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_, GetSetupInProgressHandle())
        .WillByDefault(
            Return(ByMove(std::make_unique<syncer::SyncSetupInProgressHandle>(
                base::BindRepeating(
                    &OsSyncHandlerTest::OnSetupInProgressHandleDestroyed,
                    base::Unretained(this))))));

    // Configure user settings with the sync feature on and all types enabled.
    user_settings_ = mock_sync_service_->GetMockUserSettings();
    ON_CALL(*user_settings_, GetOsSyncFeatureEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*user_settings_, IsSyncAllOsTypesEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*user_settings_, GetSelectedOsTypes())
        .WillByDefault(Return(UserSelectableOsTypeSet::All()));
    ON_CALL(*user_settings_, GetRegisteredSelectableOsTypes())
        .WillByDefault(Return(UserSelectableOsTypeSet::All()));

    handler_ = std::make_unique<OSSyncHandler>(profile());
    handler_->SetWebUIForTest(&web_ui_);
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    handler_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void OnSetupInProgressHandleDestroyed() {
    in_progress_handle_destroyed_count_++;
  }

  // Expects that an "os-sync-prefs-changed" event was sent to the WebUI and
  // returns the data passed to that event.
  const DictionaryValue* ExpectOsSyncPrefsSent() {
    const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());

    std::string event;
    EXPECT_TRUE(call_data.arg1()->GetAsString(&event));
    EXPECT_EQ(event, "os-sync-prefs-changed");

    const DictionaryValue* dictionary = nullptr;
    EXPECT_TRUE(call_data.arg2()->GetAsDictionary(&dictionary));
    return dictionary;
  }

  void NotifySyncStateChanged() {
    handler_->OnStateChanged(mock_sync_service_);
  }

  syncer::SyncUserSettingsMock* user_settings_ = nullptr;
  syncer::MockSyncService* mock_sync_service_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  content::TestWebUI web_ui_;
  TestWebUIProvider test_web_ui_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_web_ui_factory_;
  std::unique_ptr<OSSyncHandler> handler_;

  int in_progress_handle_destroyed_count_ = 0;
};

TEST_F(OsSyncHandlerTest, OsSyncPrefsSentOnNavigateToPage) {
  handler_->HandleDidNavigateToOsSyncPage(nullptr);
  ASSERT_EQ(1U, web_ui_.call_data().size());
  ExpectOsSyncPrefsSent();
}

TEST_F(OsSyncHandlerTest, OsSyncPrefsWhenFeatureIsDisabled) {
  ON_CALL(*user_settings_, GetOsSyncFeatureEnabled())
      .WillByDefault(Return(false));
  handler_->HandleDidNavigateToOsSyncPage(nullptr);
  const DictionaryValue* os_sync_prefs = ExpectOsSyncPrefsSent();
  CheckBool(os_sync_prefs, "featureEnabled", false);
}

TEST_F(OsSyncHandlerTest, OpenConfigPageBeforeSyncEngineInitialized) {
  // Sync engine is stopped initially and will start up later.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(
          Return(syncer::SyncService::TransportState::START_DEFERRED));

  // Navigate to the page.
  handler_->HandleDidNavigateToOsSyncPage(nullptr);

  // No data is sent yet, because the engine is not initialized.
  EXPECT_EQ(0U, web_ui_.call_data().size());

  // Now, act as if the SyncService has started up.
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  NotifySyncStateChanged();

  // Update for sync prefs is sent.
  EXPECT_EQ(1U, web_ui_.call_data().size());
  ExpectOsSyncPrefsSent();
}

TEST_F(OsSyncHandlerTest, NavigateAwayDestroysInProgressHandle) {
  handler_->HandleDidNavigateToOsSyncPage(nullptr);
  handler_->HandleDidNavigateAwayFromOsSyncPage(nullptr);
  EXPECT_EQ(1, in_progress_handle_destroyed_count_);
}

// Tests that signals not related to user intention to configure sync don't
// trigger sync engine start.
TEST_F(OsSyncHandlerTest, OnlyStartEngineWhenConfiguringSync) {
  ON_CALL(*mock_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  EXPECT_CALL(*user_settings_, SetSyncRequested(true)).Times(0);
  NotifySyncStateChanged();
}

TEST_F(OsSyncHandlerTest, UserDisablesFeature) {
  base::ListValue list_args;
  list_args.Append(CreateOsSyncPrefs(FEATURE_DISABLED, SYNC_ALL_OS_TYPES,
                                     UserSelectableOsTypeSet::All()));
  EXPECT_CALL(*user_settings_, SetOsSyncFeatureEnabled(false));
  handler_->HandleSetOsSyncDatatypes(&list_args);
}

TEST_F(OsSyncHandlerTest, TestSyncEverything) {
  base::ListValue list_args;
  list_args.Append(CreateOsSyncPrefs(FEATURE_ENABLED, SYNC_ALL_OS_TYPES,
                                     UserSelectableOsTypeSet::All()));
  EXPECT_CALL(*user_settings_,
              SetSelectedOsTypes(/*sync_all_os_types=*/true, _));
  handler_->HandleSetOsSyncDatatypes(&list_args);
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(OsSyncHandlerTest, TestSyncIndividualTypes) {
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    UserSelectableOsTypeSet types = {type};
    base::ListValue list_args;
    list_args.Append(
        CreateOsSyncPrefs(FEATURE_ENABLED, CHOOSE_WHAT_TO_SYNC, types));
    EXPECT_CALL(*user_settings_, SetSelectedOsTypes(false, types));

    handler_->HandleSetOsSyncDatatypes(&list_args);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
  }
}

TEST_F(OsSyncHandlerTest, TestSyncAllManually) {
  base::ListValue list_args;
  list_args.Append(CreateOsSyncPrefs(FEATURE_ENABLED, CHOOSE_WHAT_TO_SYNC,
                                     UserSelectableOsTypeSet::All()));
  EXPECT_CALL(*user_settings_,
              SetSelectedOsTypes(false, UserSelectableOsTypeSet::All()));
  handler_->HandleSetOsSyncDatatypes(&list_args);
}

TEST_F(OsSyncHandlerTest, ShowSetupSyncEverything) {
  handler_->HandleDidNavigateToOsSyncPage(nullptr);

  const DictionaryValue* dictionary = ExpectOsSyncPrefsSent();
  CheckBool(dictionary, "syncAllOsTypes", true);
  CheckBool(dictionary, "osPreferencesRegistered", true);
  CheckBool(dictionary, "printersRegistered", true);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_OS_TYPES,
                               UserSelectableOsTypeSet::All());
}

TEST_F(OsSyncHandlerTest, ShowSetupManuallySyncAll) {
  ON_CALL(*user_settings_, IsSyncAllOsTypesEnabled())
      .WillByDefault(Return(false));
  handler_->HandleDidNavigateToOsSyncPage(nullptr);

  const DictionaryValue* dictionary = ExpectOsSyncPrefsSent();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC,
                               UserSelectableOsTypeSet::All());
}

TEST_F(OsSyncHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    ON_CALL(*user_settings_, IsSyncAllOsTypesEnabled())
        .WillByDefault(Return(false));
    UserSelectableOsTypeSet types(type);
    ON_CALL(*user_settings_, GetSelectedOsTypes()).WillByDefault(Return(types));

    handler_->HandleDidNavigateToOsSyncPage(nullptr);

    const DictionaryValue* dictionary = ExpectOsSyncPrefsSent();
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types);
    Mock::VerifyAndClearExpectations(mock_sync_service_);
  }
}

}  // namespace
