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
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// These constants are used to inject the state of the Media Router action
// that would be inferred in the production code.
constexpr bool kInToolbar = true;
constexpr bool kInOverflowMenu = false;
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

    toolbar_actions_model_ =
        extensions::extension_action_test_util::CreateToolbarModelForProfile(
            profile());

    browser_action_test_util_ = BrowserActionTestUtil::Create(browser(), false);
    action_ = std::make_unique<MediaRouterAction>(
        browser(), browser_action_test_util_->GetToolbarActionsBar());

    // Pin the Media Router action to the toolbar.
    MediaRouterActionController::SetAlwaysShowActionPref(profile(), true);

    media_router::MediaRouterUIServiceFactory::GetInstance()->SetTestingFactory(
        profile()->GetOffTheRecordProfile(),
        base::BindRepeating(&BuildUIService));
  }

  void TearDown() override {
    // |identity_test_env_adaptor_| must be destroyed before the TestingProfile,
    // which occurs in BrowserWithTestWindowTest::TearDown().
    identity_test_env_adaptor_.reset();
    action_.reset();
    browser_action_test_util_.reset();
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
  identity::IdentityTestEnvironment* identity_test_env() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util_;
  std::unique_ptr<MediaRouterAction> action_;
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
  // Hide in Chrome menu / Show in toolbar
  // -----
  // Enable cloud services (checkbox)
  // Report an issue
  int expected_number_items = 9;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On all platforms except Linux, there's an additional menu item to access
  // Cast device management.
  expected_number_items++;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

  ui::SimpleMenuModel* model =
      static_cast<ui::SimpleMenuModel*>(action_->GetContextMenu());
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

// Note that "Manage devices" is always disabled on Linux.
TEST_F(MediaRouterContextualMenuUnitTest, ManageDevicesDisabledInIncognito) {
  std::unique_ptr<BrowserWindow> window(CreateBrowserWindow());
  std::unique_ptr<Browser> incognito_browser(
      CreateBrowser(profile()->GetOffTheRecordProfile(), Browser::TYPE_TABBED,
                    false, window.get()));

  action_ = std::make_unique<MediaRouterAction>(
      incognito_browser.get(),
      browser_action_test_util_->GetToolbarActionsBar());
  ui::SimpleMenuModel* model =
      static_cast<ui::SimpleMenuModel*>(action_->GetContextMenu());
  EXPECT_EQ(-1, model->GetIndexOfCommandId(IDC_MEDIA_ROUTER_MANAGE_DEVICES));
  action_.reset();
}

// "Report an issue" should be present for normal profiles but not for
// incognito.
TEST_F(MediaRouterContextualMenuUnitTest, EnableAndDisableReportIssue) {
  ui::SimpleMenuModel* model =
      static_cast<ui::SimpleMenuModel*>(action_->GetContextMenu());
  EXPECT_NE(-1, model->GetIndexOfCommandId(IDC_MEDIA_ROUTER_REPORT_ISSUE));

  std::unique_ptr<BrowserWindow> window(CreateBrowserWindow());
  std::unique_ptr<Browser> incognito_browser(
      CreateBrowser(profile()->GetOffTheRecordProfile(), Browser::TYPE_TABBED,
                    false, window.get()));

  action_ = std::make_unique<MediaRouterAction>(
      incognito_browser.get(),
      browser_action_test_util_->GetToolbarActionsBar());
  model = static_cast<ui::SimpleMenuModel*>(action_->GetContextMenu());
  EXPECT_EQ(-1, model->GetIndexOfCommandId(IDC_MEDIA_ROUTER_REPORT_ISSUE));
  action_.reset();
}

// Tests whether the cloud services item is correctly toggled. This menu item
// is only availble on official Chrome builds.
// TODO(takumif): Add a test case that checks that the cloud services dialog is
// shown when the services are enabled for the first time.
TEST_F(MediaRouterContextualMenuUnitTest, ToggleCloudServicesItem) {
  // The Media Router Action has a getter for the model, but not the delegate.
  // Create the MediaRouterContextualMenu ui::SimpleMenuModel::Delegate here.
  MediaRouterContextualMenu menu(browser(), kInToolbar, kShownByPolicy,
                                 &observer_);

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
  MediaRouterContextualMenu menu(browser(), kInToolbar, kShownByPolicy,
                                 &observer_);

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
  MediaRouterContextualMenu menu(browser(), kInToolbar, kShownByUser,
                                 &observer_);

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
  MediaRouterContextualMenu menu(browser(), kInToolbar, kShownByPolicy,
                                 &observer_);

  // The item "Added by your administrator" should be shown disabled.
  EXPECT_TRUE(menu.IsCommandIdVisible(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));

  // The checkbox item "Always show icon" should not be shown.
  EXPECT_FALSE(HasCommandId(menu.menu_model(),
                            IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));
}

TEST_F(MediaRouterContextualMenuUnitTest, HideActionInOverflowItem) {
  MediaRouterContextualMenu menu(browser(), kInToolbar, kShownByUser,
                                 &observer_);

  // When the action icon is in the toolbar, this menu item should say "Hide
  // in Chrome menu".
  const base::string16& menu_item_label = menu.menu_model()->GetLabelAt(
      menu.menu_model()->GetIndexOfCommandId(IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR));
  EXPECT_EQ(menu_item_label,
            l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU));
}

TEST_F(MediaRouterContextualMenuUnitTest, ShowActionInToolbarItem) {
  MediaRouterContextualMenu menu(browser(), kInOverflowMenu, kShownByUser,
                                 &observer_);

  // When the action icon is in the overflow menu, this menu item should say
  // "Show in toolbar".
  const base::string16& menu_item_label = menu.menu_model()->GetLabelAt(
      menu.menu_model()->GetIndexOfCommandId(IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR));
  EXPECT_EQ(menu_item_label,
            l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR));
}

TEST_F(MediaRouterContextualMenuUnitTest, NotifyActionController) {
  EXPECT_CALL(observer_, OnContextMenuShown());
  auto menu = std::make_unique<MediaRouterContextualMenu>(
      browser(), kInToolbar, kShownByUser, &observer_);
  menu->OnMenuWillShow(menu->menu_model());

  EXPECT_CALL(observer_, OnContextMenuHidden());
  menu->MenuClosed(menu->menu_model());
  menu.reset();
}
