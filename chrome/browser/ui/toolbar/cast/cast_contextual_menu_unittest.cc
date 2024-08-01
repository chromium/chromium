// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/toolbar/cast/cast_contextual_menu.h"
#include "chrome/browser/ui/toolbar/cast/mock_cast_toolbar_button_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::NiceMock;

namespace {

// These constants are used to inject the state of the Cast toolbar icon
// that would be inferred in the production code.
constexpr bool kShownByPolicy = true;
constexpr bool kShownByUser = false;

bool HasCommandId(ui::MenuModel* menu_model, int command_id) {
  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (menu_model->GetCommandIdAt(i) == command_id)
      return true;
  }
  return false;
}

std::unique_ptr<KeyedService> BuildUIService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto controller =
      std::make_unique<NiceMock<MockCastToolbarButtonController>>(profile);
  return std::make_unique<media_router::MediaRouterUIService>(
      profile, std::move(controller));
}

class MockCastContextualMenuObserver
    : public CastContextualMenu::Observer {
 public:
  MOCK_METHOD(void, OnContextMenuShown, ());
  MOCK_METHOD(void, OnContextMenuHidden, ());
};

}  // namespace

class CastContextualMenuUnitTest : public BrowserWithTestWindowTest {
 public:
  CastContextualMenuUnitTest() = default;
  CastContextualMenuUnitTest(const CastContextualMenuUnitTest&) =
      delete;
  CastContextualMenuUnitTest& operator=(
      const CastContextualMenuUnitTest&) = delete;
  ~CastContextualMenuUnitTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    extensions::LoadErrorReporter::Init(true);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    // Pin the Cast icon to the toolbar.
    CastToolbarButtonController::SetAlwaysShowActionPref(profile(), true);

    media_router::MediaRouterUIServiceFactory::GetInstance()->SetTestingFactory(
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        base::BindRepeating(&BuildUIService));
  }

  void TearDown() override {
    // |identity_test_env_adaptor_| must be destroyed before the TestingProfile,
    // which occurs in BrowserWithTestWindowTest::TearDown().
    identity_test_env_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                 media_router::ChromeMediaRouterFactory::GetInstance(),
                 base::BindRepeating(&media_router::MockMediaRouter::Create)},
             TestingProfile::TestingFactory{
                 media_router::MediaRouterUIServiceFactory::GetInstance(),
                 base::BindRepeating(&BuildUIService)}});
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  raw_ptr<ToolbarActionsModel> toolbar_actions_model_ = nullptr;
  MockCastContextualMenuObserver observer_;
};

// Tests the basic state of the contextual menu.
TEST_F(CastContextualMenuUnitTest, Basic) {
  // About
  // -----
  // Learn more
  // Help
  // Always show icon (checkbox)
  // Optimize fullscreen videos (checkbox)
  // -----
  // Report an issue

  // Number of menu items, including separators.
  size_t expected_number_items = 6;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_number_items += 2;
#endif

  CastContextualMenu menu(browser(), kShownByUser, &observer_);
  std::unique_ptr<ui::SimpleMenuModel> model = menu.CreateMenuModel();
  EXPECT_EQ(model->GetItemCount(), expected_number_items);

  for (size_t i = 0; i < expected_number_items; ++i) {
    EXPECT_TRUE(model->IsEnabledAt(i));
    EXPECT_TRUE(model->IsVisibleAt(i));
  }

  // Set up an authenticated account.
  identity_test_env()->SetPrimaryAccount("foo@bar.com",
                                         signin::ConsentLevel::kSync);

  // Run the same checks as before. All existing menu items should be now
  // enabled and visible.
  EXPECT_EQ(model->GetItemCount(), expected_number_items);
  for (size_t i = 0; i < expected_number_items; ++i) {
    EXPECT_TRUE(model->IsEnabledAt(i));
    EXPECT_TRUE(model->IsVisibleAt(i));
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// "Report an issue" should be present for normal profiles as well as for
// incognito.
TEST_F(CastContextualMenuUnitTest, EnableAndDisableReportIssue) {
  CastContextualMenu menu(browser(), kShownByPolicy, &observer_);
  EXPECT_TRUE(
      menu.CreateMenuModel()
          ->GetIndexOfCommandId(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE)
          .has_value());

  std::unique_ptr<BrowserWindow> window(CreateBrowserWindow());
  std::unique_ptr<Browser> incognito_browser(
      CreateBrowser(profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                    Browser::TYPE_NORMAL, false, window.get()));

  CastContextualMenu incognito_menu(incognito_browser.get(),
                                           kShownByPolicy, &observer_);
  EXPECT_TRUE(
      incognito_menu.CreateMenuModel()
          ->GetIndexOfCommandId(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE)
          .has_value());
}
#endif

TEST_F(CastContextualMenuUnitTest, ToggleMediaRemotingItem) {
  CastContextualMenu menu(browser(), kShownByPolicy, &observer_);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));
  EXPECT_TRUE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING));
  EXPECT_FALSE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled));
}

TEST_F(CastContextualMenuUnitTest, ToggleAlwaysShowIconItem) {
  CastContextualMenu menu(browser(), kShownByUser, &observer_);

  // Whether the option is checked should reflect the pref.
  CastToolbarButtonController::SetAlwaysShowActionPref(profile(), true);
  EXPECT_TRUE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  CastToolbarButtonController::SetAlwaysShowActionPref(profile(), false);
  EXPECT_FALSE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  // Executing the option should toggle the pref.
  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_TRUE(CastToolbarButtonController::GetAlwaysShowActionPref(profile()));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_FALSE(CastToolbarButtonController::GetAlwaysShowActionPref(profile()));
}

TEST_F(CastContextualMenuUnitTest, ActionShownByPolicy) {
  // Create a contextual menu for an icon shown by administrator policy.
  CastContextualMenu menu(browser(), kShownByPolicy, &observer_);

  // The item "Added by your administrator" should be shown disabled.
  EXPECT_TRUE(menu.IsCommandIdVisible(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));

  // The checkbox item "Always show icon" should not be shown.
  EXPECT_FALSE(HasCommandId(menu.CreateMenuModel().get(),
                            IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));
}

TEST_F(CastContextualMenuUnitTest, NotifyActionController) {
  EXPECT_CALL(observer_, OnContextMenuShown());
  auto menu = std::make_unique<CastContextualMenu>(
      browser(), kShownByUser, &observer_);
  std::unique_ptr<ui::SimpleMenuModel> model = menu->CreateMenuModel();
  menu->OnMenuWillShow(model.get());

  EXPECT_CALL(observer_, OnContextMenuHidden());
  menu->MenuClosed(model.get());
  menu.reset();
}
