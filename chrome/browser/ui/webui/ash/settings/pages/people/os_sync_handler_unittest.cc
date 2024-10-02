// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/os_sync_handler.h"

#include <memory>
#include <string>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::TestWebUI;
using syncer::SyncService;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;
using syncer::UserSelectableTypeSet;

namespace content {
class BrowserContext;
}

namespace ash {

namespace {

using ::testing::Optional;

enum SyncAllConfig { SYNC_ALL_OS_TYPES, CHOOSE_WHAT_TO_SYNC };

// Creates a dictionary with the key/value pairs appropriate for a call to
// HandleSetOsSyncDatatypes().
base::Value::Dict CreateOsSyncPrefs(SyncAllConfig sync_all,
                                    UserSelectableOsTypeSet types,
                                    bool wallpaper_enabled) {
  base::Value::Dict result;
  result.Set("syncAllOsTypes", sync_all == SYNC_ALL_OS_TYPES);
  // Add all of our data types.
  result.Set("osAppsSynced", types.Has(UserSelectableOsType::kOsApps));
  result.Set("osPreferencesSynced",
             types.Has(UserSelectableOsType::kOsPreferences));
  result.Set("osWifiConfigurationsSynced",
             types.Has(UserSelectableOsType::kOsWifiConfigurations));
  result.Set("wallpaperEnabled",
             sync_all == SYNC_ALL_OS_TYPES || wallpaper_enabled);
  return result;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|.
void CheckBool(const base::Value::Dict& dictionary,
               const std::string& key,
               bool expected_value) {
  EXPECT_THAT(dictionary.FindBool(key), Optional(expected_value))
      << "Key: " << key;
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the JS layer.
void CheckConfigDataTypeArguments(const base::Value::Dict& dictionary,
                                  SyncAllConfig config,
                                  UserSelectableOsTypeSet types,
                                  bool wallpaper_enabled) {
  CheckBool(dictionary, "syncAllOsTypes", config == SYNC_ALL_OS_TYPES);
  CheckBool(dictionary, "osAppsSynced",
            types.Has(UserSelectableOsType::kOsApps));
  CheckBool(dictionary, "osPreferencesSynced",
            types.Has(UserSelectableOsType::kOsPreferences));
  CheckBool(dictionary, "osWifiConfigurationsSynced",
            types.Has(UserSelectableOsType::kOsWifiConfigurations));
  CheckBool(dictionary, "wallpaperEnabled",
            config == SYNC_ALL_OS_TYPES || wallpaper_enabled);
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class TestWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    return std::make_unique<content::WebUIController>(web_ui);
  }
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class OsSyncHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  OsSyncHandlerTest() = default;
  ~OsSyncHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Sign in the user.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_adaptor_->identity_test_env()->SetPrimaryAccount(
        "test@gmail.com", signin::ConsentLevel::kSync);

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));
    user_settings_ = sync_service_->GetUserSettings();

    auto handler = std::make_unique<OSSyncHandler>(profile());
    handler_ = handler.get();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->AddMessageHandler(std::move(handler));
    web_ui_->set_web_contents(web_contents());
  }

  void TearDown() override {
    web_ui_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  // Expects that an "os-sync-prefs-changed" event was sent to the WebUI and
  // returns the data passed to that event.
  base::Value::Dict ExpectOsSyncPrefsSent() {
    const TestWebUI::CallData& call_data = *web_ui_->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());

    EXPECT_TRUE(call_data.arg1());
    const std::string* event = call_data.arg1()->GetIfString();
    EXPECT_TRUE(event);
    EXPECT_EQ(*event, "os-sync-prefs-changed");

    EXPECT_TRUE(call_data.arg2());
    const base::Value::Dict* dict = call_data.arg2()->GetIfDict();
    EXPECT_TRUE(dict);
    return dict->Clone();
  }

  bool GetWallperEnabledPref() {
    return profile()->GetPrefs()->GetBoolean(settings::prefs::kSyncOsWallpaper);
  }

  void SetWallperEnabledPref(bool enabled) {
    return profile()->GetPrefs()->SetBoolean(settings::prefs::kSyncOsWallpaper,
                                             enabled);
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  raw_ptr<syncer::TestSyncService, DanglingUntriaged> sync_service_ = nullptr;
  raw_ptr<syncer::SyncUserSettings, DanglingUntriaged> user_settings_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<TestWebUI> web_ui_;
  TestWebUIProvider test_web_ui_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_web_ui_factory_;
  raw_ptr<OSSyncHandler, DanglingUntriaged> handler_;

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(OsSyncHandlerTest, OsSyncPrefsSentOnNavigateToPage) {
  handler_->HandleDidNavigateToOsSyncPage(base::Value::List());

  ASSERT_EQ(1U, web_ui_->call_data().size());
  const TestWebUI::CallData& call_data = *web_ui_->call_data().back();

  std::string event_name = call_data.arg1()->GetString();
  EXPECT_EQ(event_name, "os-sync-prefs-changed");
}

TEST_F(OsSyncHandlerTest, OpenConfigPageBeforeSyncEngineInitialized) {
  // Sync engine is stopped initially and will start up later.
  sync_service_->SetMaxTransportState(
      SyncService::TransportState::START_DEFERRED);

  // Navigate to the page.
  handler_->HandleDidNavigateToOsSyncPage(base::Value::List());

  // No data is sent yet, because the engine is not initialized.
  EXPECT_EQ(0U, web_ui_->call_data().size());

  // Now, act as if the SyncService has started up.
  sync_service_->SetMaxTransportState(SyncService::TransportState::ACTIVE);
  handler_->OnStateChanged(sync_service_);

  // Update for sync prefs is sent.
  ASSERT_EQ(1U, web_ui_->call_data().size());
  const TestWebUI::CallData& call_data = *web_ui_->call_data().back();

  std::string event_name = call_data.arg1()->GetString();
  EXPECT_EQ(event_name, "os-sync-prefs-changed");
}

TEST_F(OsSyncHandlerTest, TestSyncEverything) {
  base::Value::List list_args;
  list_args.Append(CreateOsSyncPrefs(SYNC_ALL_OS_TYPES,
                                     UserSelectableOsTypeSet::All(),
                                     /*wallpaper_enabled=*/true));
  handler_->HandleSetOsSyncDatatypes(list_args);
  EXPECT_TRUE(user_settings_->IsSyncAllOsTypesEnabled());
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(OsSyncHandlerTest, TestSyncIndividualTypes) {
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    UserSelectableOsTypeSet types = {type};
    base::Value::List list_args;
    list_args.Append(CreateOsSyncPrefs(CHOOSE_WHAT_TO_SYNC, types,
                                       /*wallpaper_enabled=*/false));

    handler_->HandleSetOsSyncDatatypes(list_args);
    EXPECT_FALSE(user_settings_->IsSyncAllOsTypesEnabled());
    EXPECT_EQ(types, user_settings_->GetSelectedOsTypes());
  }

  // Special case for wallpaper.
  base::Value::List list_args;
  list_args.Append(CreateOsSyncPrefs(CHOOSE_WHAT_TO_SYNC, /*types=*/{},
                                     /*wallpaper_enabled=*/true));
  handler_->HandleSetOsSyncDatatypes(list_args);
  EXPECT_FALSE(user_settings_->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(GetWallperEnabledPref());
}

TEST_F(OsSyncHandlerTest, TestSyncAllManually) {
  base::Value::List list_args;
  list_args.Append(CreateOsSyncPrefs(CHOOSE_WHAT_TO_SYNC,
                                     UserSelectableOsTypeSet::All(),
                                     /*wallpaper_enabled=*/true));
  handler_->HandleSetOsSyncDatatypes(list_args);
  EXPECT_FALSE(user_settings_->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(UserSelectableOsTypeSet::All(),
            user_settings_->GetSelectedOsTypes());
  EXPECT_TRUE(GetWallperEnabledPref());
}

TEST_F(OsSyncHandlerTest, ShowSetupSyncEverything) {
  user_settings_->SetSelectedOsTypes(/*sync_all_os_types=*/true,
                                     UserSelectableOsTypeSet::All());
  SetWallperEnabledPref(true);
  handler_->HandleDidNavigateToOsSyncPage(base::Value::List());

  base::Value::Dict dictionary = ExpectOsSyncPrefsSent();
  CheckBool(dictionary, "syncAllOsTypes", true);
  CheckBool(dictionary, "osAppsRegistered", true);
  CheckBool(dictionary, "osPreferencesRegistered", true);
  CheckBool(dictionary, "osWifiConfigurationsRegistered", true);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_OS_TYPES,
                               UserSelectableOsTypeSet::All(),
                               /*wallpaper_enabled=*/true);
}

TEST_F(OsSyncHandlerTest, ShowSetupManuallySyncAll) {
  user_settings_->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                     UserSelectableOsTypeSet::All());
  SetWallperEnabledPref(true);
  handler_->HandleDidNavigateToOsSyncPage(base::Value::List());

  base::Value::Dict dictionary = ExpectOsSyncPrefsSent();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC,
                               UserSelectableOsTypeSet::All(),
                               /*wallpaper_enabled=*/true);
}

TEST_F(OsSyncHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    const UserSelectableOsTypeSet types = {type};
    user_settings_->SetSelectedOsTypes(/*sync_all_os_types=*/false, types);
    handler_->HandleDidNavigateToOsSyncPage(base::Value::List());

    base::Value::Dict dictionary = ExpectOsSyncPrefsSent();
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types,
                                 /*wallpaper_enabled=*/false);
  }

  // Special case for wallpaper.
  user_settings_->SetSelectedOsTypes(/*sync_all_os_types=*/false, /*types=*/{});
  SetWallperEnabledPref(true);
  handler_->HandleDidNavigateToOsSyncPage(base::Value::List());
  base::Value::Dict dictionary = ExpectOsSyncPrefsSent();
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, /*types=*/{},
                               /*wallpaper_enabled=*/true);
}

TEST_F(OsSyncHandlerTest, OpenBrowserSyncSettings) {
  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(
          GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSyncSetupSubPage),
          ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          ash::NewWindowDelegate::Disposition::kSwitchToTab));
  base::Value::List empty_args;
  web_ui_->HandleReceivedMessage("OpenBrowserSyncSettings", empty_args);
}

}  // namespace

}  // namespace ash
