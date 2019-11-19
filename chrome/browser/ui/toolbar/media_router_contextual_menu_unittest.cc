// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// These constants are used to inject the state of the Cast toolbar icon
// that would be inferred in the production code.
constexpr bool kShownByPolicy = true;
constexpr bool kShownByUser = false;

bool HasCommandId(ui::MenuModel* menu_model, int command_id) {
  for (int i = 0; i < menu_model->GetItemCount(); i++) {
    if (menu_model->GetCommandIdAt(i) == command_id)
      return true;
  }
  return false;
}

std::unique_ptr<KeyedService> BuildUIService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto controller = std::make_unique<MockMediaRouterActionController>(profile);
  return std::make_unique<media_router::MediaRouterUIService>(
      profile, std::move(controller));
}

class MockMediaRouterContextualMenuObserver
    : public MediaRouterContextualMenu::Observer {
 public:
  MOCK_METHOD0(OnContextMenuShown, void());
  MOCK_METHOD0(OnContextMenuHidden, void());
};

}  // namespace

class MediaRouterContextualMenuUnitTest : public BrowserWithTestWindowTest {
 public:
  MediaRouterContextualMenuUnitTest() {}
  ~MediaRouterContextualMenuUnitTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    extensions::LoadErrorReporter::Init(true);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    // Pin the Cast icon to the toolbar.
    MediaRouterActionController::SetAlwaysShowActionPref(profile(), true);

    media_router::MediaRouterUIServiceFactory::GetInstance()->SetTestingFactory(
        profile()->GetOffTheRecordProfile(),
        base::BindRepeating(&BuildUIService));
  }

  void TearDown() override {
    // |identity_test_env_adaptor_| must be destroyed before the TestingProfile,
    // which occurs in BrowserWithTestWindowTest::TearDown().
    identity_test_env_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {media_router::MediaRouterFactory::GetInstance(),
         base::BindRepeating(&media_router::MockMediaRouter::Create)},
        {media_router::MediaRouterUIServiceFactory::GetInstance(),
         base::BindRepeating(&BuildUIService)}};

    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);

    return factories;
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  ToolbarActionsModel* toolbar_actions_model_ = nullptr;
  MockMediaRouterContextualMenuObserver observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenuUnitTest);
};

// Tests the basic state of the contextual menu.
TEST_F(MediaRouterContextualMenuUnitTest, Basic) {
  // About
  // -----
  // Learn more
  // Help
  // Always show icon (checkbox)
  // Optimize fullscreen videos (checkbox)
  // -----
  // Enable cloud services (checkbox)
  // Report an issue
  int expected_number_items = 9;

  MediaRouterContextualMenu menu(browser(), kShownByUser, &observer_);
  ui::SimpleMenuModel* model = menu.menu_model();
  // Verify the number of menu items, including separators.
  EXPECT_EQ(model->GetItemCount(), expected_number_items);

  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model->IsEnabledAt(i));

    // The cloud services toggle exists and is enabled, but not visible until
    // the user has authenticated their account.
    const bool expected_visibility =
        model->GetCommandIdAt(i) != IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE;
    EXPECT_EQ(expected_visibility, model->IsVisibleAt(i));
  }

  // Set up an authenticated account.
  identity_test_env()->SetPrimaryAccount("foo@bar.com");

  // Run the same checks as before. All existing menu items should be now
  // enabled and visible.
  EXPECT_EQ(model->GetItemCount(), expected_number_items);
  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model->IsEnabledAt(i));
    EXPECT_TRUE(model->IsVisibleAt(i));
  }
}

// "Report an issue" should be present for normal profiles but not for
// incognito.
TEST_F(MediaRouterContextualMenuUnitTest, EnableAndDisableReportIssue) {
  MediaRouterContextualMenu menu(browser(), kShownByPolicy, &observer_);
  EXPECT_NE(-1, menu.menu_model()->GetIndexOfCommandId(
                    IDC_MEDIA_ROUTER_REPORT_ISSUE));

  std::unique_ptr<BrowserWindow> window(CreateBrowserWindow());
  std::unique_ptr<Browser> incognito_browser(
      CreateBrowser(profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL,
                    false, window.get()));

  MediaRouterContextualMenu incognito_menu(incognito_browser.get(),
                                           kShownByPolicy, &observer_);
  EXPECT_EQ(-1, incognito_menu.menu_model()->GetIndexOfCommandId(
                    IDC_MEDIA_ROUTER_REPORT_ISSUE));
}

// Tests whether the cloud services item is correctly toggled. This menu item
// is only availble on official Chrome builds.
// TODO(takumif): Add a test case that checks that the cloud services dialog is
// shown when the services are enabled for the first time.
TEST_F(MediaRouterContextualMenuUnitTest, ToggleCloudServicesItem) {
  // The Cast toolbar icon has a getter for the model, but not the delegate.
  // Create the MediaRouterContextualMenu ui::SimpleMenuModel::Delegate here.
  MediaRouterContextualMenu menu(browser(), kShownByPolicy, &observer_);

  // Set up an authenticated account such that the cloud services menu item is
  // surfaced. Whether or not it is surfaced is tested in the "Basic" test.
  identity_test_env()->SetPrimaryAccount("foo@bar.com");

  // Set this preference so that the cloud services can be enabled without
  // showing the opt-in dialog.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kMediaRouterCloudServicesPrefSet, true);

  // By default, the command is not checked.
  EXPECT_FALSE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));
}

TEST_F(MediaRouterContextualMenuUnitTest, ToggleMediaRemotingItem) {
  MediaRouterContextualMenu menu(browser(), kShownByPolicy, &observer_);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kMediaRouterMediaRemotingEnabled, false);
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));
  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kMediaRouterMediaRemotingEnabled));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));
  EXPECT_FALSE(
      pref_service->GetBoolean(prefs::kMediaRouterMediaRemotingEnabled));
}

TEST_F(MediaRouterContextualMenuUnitTest, ToggleAlwaysShowIconItem) {
  MediaRouterContextualMenu menu(browser(), kShownByUser, &observer_);

  // Whether the option is checked should reflect the pref.
  MediaRouterActionController::SetAlwaysShowActionPref(profile(), true);
  EXPECT_TRUE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  MediaRouterActionController::SetAlwaysShowActionPref(profile(), false);
  EXPECT_FALSE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  // Executing the option should toggle the pref.
  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_TRUE(MediaRouterActionController::GetAlwaysShowActionPref(profile()));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_FALSE(MediaRouterActionController::GetAlwaysShowActionPref(profile()));
}

TEST_F(MediaRouterContextualMenuUnitTest, ActionShownByPolicy) {
  // Create a contextual menu for an icon shown by administrator policy.
  MediaRouterContextualMenu menu(browser(), kShownByPolicy, &observer_);

  // The item "Added by your administrator" should be shown disabled.
  EXPECT_TRUE(menu.IsCommandIdVisible(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));

  // The checkbox item "Always show icon" should not be shown.
  EXPECT_FALSE(HasCommandId(menu.menu_model(),
                            IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));
}

TEST_F(MediaRouterContextualMenuUnitTest, NotifyActionController) {
  EXPECT_CALL(observer_, OnContextMenuShown());
  auto menu = std::make_unique<MediaRouterContextualMenu>(
      browser(), kShownByUser, &observer_);
  menu->OnMenuWillShow(menu->menu_model());

  EXPECT_CALL(observer_, OnContextMenuHidden());
  menu->MenuClosed(menu->menu_model());
  menu.reset();
}
