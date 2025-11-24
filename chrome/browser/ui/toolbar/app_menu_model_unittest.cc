// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using ::testing::_;

namespace {

// Error class has a menu item.
class MenuError : public GlobalError {
 public:
  explicit MenuError(int command_id) : command_id_(command_id) {}

  MenuError(const MenuError&) = delete;
  MenuError& operator=(const MenuError&) = delete;

  int execute_count() { return execute_count_; }

  bool HasMenuItem() override { return true; }
  int MenuItemCommandID() override { return command_id_; }
  std::u16string MenuItemLabel() override { return std::u16string(); }
  void ExecuteMenuItem(Browser* browser) override { execute_count_++; }

  bool HasBubbleView() override { return false; }
  bool HasShownBubbleView() override { return false; }
  void ShowBubbleView(Browser* browser) override { ADD_FAILURE(); }
  GlobalErrorBubbleViewBase* GetBubbleView() override { return nullptr; }

 private:
  int command_id_;
  int execute_count_ = 0;
};

class FakeIconDelegate : public AppMenuIconController::Delegate {
 public:
  FakeIconDelegate() = default;

  // AppMenuIconController::Delegate:
  void UpdateTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) override {}
};

}  // namespace

class AppMenuModelTest : public BrowserWithTestWindowTest,
                         public ui::AcceleratorProvider {
 public:
  AppMenuModelTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    safety_hub_test_util::CreateRevokedPermissionsService(browser()->profile());
    safety_hub_test_util::CreateNotificationPermissionsReviewService(
        browser()->profile());
  }

  AppMenuModelTest(const AppMenuModelTest&) = delete;
  AppMenuModelTest& operator=(const AppMenuModelTest&) = delete;

  ~AppMenuModelTest() override = default;

  // Don't handle accelerators.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return false;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Copies parts of MenuModelTest::Delegate and combines them with the
// AppMenuModel since AppMenuModel is now a SimpleMenuModel::Delegate and
// not derived from SimpleMenuModel.
class TestAppMenuModel : public AppMenuModel {
 public:
  TestAppMenuModel(ui::AcceleratorProvider* provider,
                   Browser* browser,
                   AppMenuIconController* app_menu_icon_controller)
      : AppMenuModel(provider, browser, app_menu_icon_controller) {}

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    bool val = AppMenuModel::IsCommandIdChecked(command_id);
    if (val) {
      checked_count_++;
    }
    return val;
  }

  bool IsCommandIdEnabled(int command_id) const override {
    ++enable_count_;
    return true;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    ++execute_count_;
  }

  int execute_count_ = 0;
  mutable int checked_count_ = 0;
  mutable int enable_count_ = 0;
};

class TestLogMetricsAppMenuModel : public AppMenuModel {
 public:
  TestLogMetricsAppMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser)
      : AppMenuModel(provider, browser) {}

  void LogMenuAction(AppMenuAction action_id) override { log_metrics_count_++; }

  int log_metrics_count_ = 0;
};

TEST_F(AppMenuModelTest, Basics) {
  // Simulate that an update is available to ensure that the menu includes the
  // upgrade item for platforms that support it.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  detector->set_upgrade_notification_stage(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  detector->NotifyUpgrade();
  EXPECT_TRUE(detector->notify_upgrade());

  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestAppMenuModel model(this, browser(), &app_menu_icon_controller);
  model.Init();
  size_t item_count = model.GetItemCount();

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(item_count, 10u);

  // Verify that the upgrade item is visible if supported.
  EXPECT_EQ(browser_defaults::kShowUpgradeMenuItem,
            model.GetIndexOfCommandId(IDC_UPGRADE_DIALOG).has_value());

  // Execute a couple of the items and make sure it gets back to our delegate.
  // We can't use CountEnabledExecutable() here because the encoding menu's
  // delegate is internal, it doesn't use the one we pass in.
  // Note: the second item in the menu may be a separator if the browser
  // supports showing upgrade status in the app menu.
  size_t item_index = 1;
  if (model.GetTypeAt(item_index) == ui::MenuModel::TYPE_SEPARATOR) {
    ++item_index;
  }
  model.ActivatedAt(item_index);
  EXPECT_TRUE(model.IsEnabledAt(item_index));
  // Make sure to use the index that is not separator in all configurations.
  model.ActivatedAt(item_count - 1);
  EXPECT_TRUE(model.IsEnabledAt(item_count - 1));

  EXPECT_EQ(model.execute_count_, 2);
  EXPECT_EQ(model.enable_count_, 2);

  model.execute_count_ = 0;
  model.enable_count_ = 0;

  // Choose something from the bookmark submenu and make sure it makes it back
  // to the delegate as well.
  size_t bookmarks_model_index =
      model.GetIndexOfCommandId(IDC_BOOKMARKS_MENU).value();

  EXPECT_GT(bookmarks_model_index, 0u);
  ui::MenuModel* bookmarks_model =
      model.GetSubmenuModelAt(bookmarks_model_index);
  EXPECT_TRUE(bookmarks_model);
  // The bookmarks model may be empty until we tell it we're going to show it.
  bookmarks_model->MenuWillShow();
  EXPECT_GT(bookmarks_model->GetItemCount(), 1u);

  // Bookmark manager item.
  bookmarks_model->ActivatedAt(4);
  EXPECT_TRUE(bookmarks_model->IsEnabledAt(4));
  EXPECT_EQ(model.execute_count_, 1);
  EXPECT_EQ(model.enable_count_, 1);
}

// Tests global error menu items in the app menu.
TEST_F(AppMenuModelTest, GlobalError) {
  // Make sure services required for tests are initialized.
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  const int command1 = 1234567;
  MenuError* error1 = new MenuError(command1);
  service->AddGlobalError(base::WrapUnique(error1));
  const int command2 = 1234568;
  MenuError* error2 = new MenuError(command2);
  service->AddGlobalError(base::WrapUnique(error2));

  AppMenuModel model(this, browser());
  model.Init();
  std::optional<size_t> index1 = model.GetIndexOfCommandId(command1);
  ASSERT_TRUE(index1.has_value());
  std::optional<size_t> index2 = model.GetIndexOfCommandId(command2);
  ASSERT_TRUE(index2.has_value());

  EXPECT_TRUE(model.IsEnabledAt(index1.value()));
  EXPECT_EQ(0, error1->execute_count());
  model.ActivatedAt(index1.value());
  EXPECT_EQ(1, error1->execute_count());

  EXPECT_TRUE(model.IsEnabledAt(index2.value()));
  EXPECT_EQ(0, error2->execute_count());
  model.ActivatedAt(index2.value());
  EXPECT_EQ(1, error1->execute_count());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(AppMenuModelTest, DefaultBrowserPrompt) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestAppMenuModel model(this, browser(), &app_menu_icon_controller);
  model.Init();

  EXPECT_TRUE(
      model.GetIndexOfCommandId(IDC_SET_BROWSER_AS_DEFAULT).has_value());

  size_t default_prompt_index =
      model.GetIndexOfCommandId(IDC_SET_BROWSER_AS_DEFAULT).value();
  EXPECT_TRUE(model.IsEnabledAt(default_prompt_index));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(AppMenuModelTest, PerformanceItem) {
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  ASSERT_TRUE(toolModel.GetIndexOfCommandId(IDC_PERFORMANCE));
  size_t performance_index =
      toolModel.GetIndexOfCommandId(IDC_PERFORMANCE).value();
  EXPECT_TRUE(toolModel.IsEnabledAt(performance_index));
}

TEST_F(AppMenuModelTest, CustomizeChromeItem) {
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel tool_model(&model, browser());
  ASSERT_TRUE(
      tool_model.GetIndexOfCommandId(IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL));
  size_t customize_chrome_index =
      tool_model.GetIndexOfCommandId(IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL)
          .value();
  EXPECT_TRUE(tool_model.IsEnabledAt(customize_chrome_index));
}

TEST_F(AppMenuModelTest, CustomizeChromeLogMetrics) {
  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  model.ExecuteCommand(IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}

TEST_F(AppMenuModelTest, OrganizeTabsItem) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {features::kTabOrganization, features::kTabOrganizationAppMenuItem}, {});

  TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  size_t organize_tabs_index =
      toolModel.GetIndexOfCommandId(IDC_ORGANIZE_TABS).value();
  EXPECT_TRUE(toolModel.IsEnabledAt(organize_tabs_index));
}

TEST_F(AppMenuModelTest, DeclutterTabsItem) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(features::kTabstripDeclutter);
  TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  size_t declutter_tabs_index =
      toolModel.GetIndexOfCommandId(IDC_DECLUTTER_TABS).value();
  EXPECT_TRUE(toolModel.IsEnabledAt(declutter_tabs_index));
  model.ExecuteCommand(IDC_DECLUTTER_TABS, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(AppMenuModelTest, GlicItem) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {features::kGlic, features::kTabstripComboButton, features::kGlicRollout},
      {});

  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  model.ExecuteCommand(IDC_OPEN_GLIC, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}
#endif

TEST_F(AppMenuModelTest, DoNotShowShareSubMenuItem) {
  PrefService* prefs = browser()->profile()->GetPrefs();
#if !BUILDFLAG(IS_CHROMEOS)
  prefs->SetBoolean(prefs::kDesktopSharingHubEnabled, false);
#endif
  prefs->SetBoolean(prefs::kDisableScreenshots, true);

  AppMenuModel model(this, browser());
  model.Init();

  ASSERT_TRUE(model.GetIndexOfCommandId(IDC_SAVE_AND_SHARE_MENU));
  ui::MenuModel* submenu = model.GetSubmenuModelAt(
      model.GetIndexOfCommandId(IDC_SAVE_AND_SHARE_MENU).value());
  ASSERT_NE(submenu, nullptr);

  size_t expected_item_count = 7;
  if (!sharing_hub::SharingIsDisabledByPolicy(browser()->profile()) ||
      sharing_hub::DesktopScreenshotsFeatureEnabled(browser()->profile())) {
    expected_item_count += 2;
    if (!sharing_hub::SharingIsDisabledByPolicy(browser()->profile())) {
      expected_item_count += 3;
    }
    if (sharing_hub::DesktopScreenshotsFeatureEnabled(browser()->profile())) {
      expected_item_count += 1;
    }
  }
  EXPECT_EQ(expected_item_count, submenu->GetItemCount());
}

TEST_F(AppMenuModelTest, ModelHasIcons) {
  // Skip the items that are either not supposed to have an icon, or are not
  // ready to be tested. Remove items once they're ready for testing.
  static const std::vector<int> skip_commands = {
      IDC_RECENT_TABS_NO_DEVICE_TABS, IDC_ABOUT,
      RecentTabsSubMenuModel::GetDisabledRecentlyClosedHeaderCommandId(),
      IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE, IDC_TAKE_SCREENSHOT};
  AppMenuModel model(this, browser());
  model.Init();

  const auto check_for_icons = [](std::u16string menu_name,
                                  ui::MenuModel* model) -> void {
    auto check_for_icons_impl = [](std::u16string menu_name,
                                   ui::MenuModel* model,
                                   auto& check_for_icons_ref) -> void {
      // Except where noted by the above vector, all menu items in CR2023 must
      // have icons.
      for (size_t i = 0; i < model->GetItemCount(); ++i) {
        auto menu_type = model->GetTypeAt(i);
        if (menu_type != ui::MenuModel::TYPE_ACTIONABLE_SUBMENU &&
            menu_type != ui::MenuModel::TYPE_SUBMENU &&
            std::find(skip_commands.cbegin(), skip_commands.cend(),
                      model->GetCommandIdAt(i)) != skip_commands.cend()) {
          continue;
        }
        if (menu_type != ui::MenuModel::TYPE_SEPARATOR &&
            menu_type != ui::MenuModel::TYPE_TITLE) {
          EXPECT_TRUE(!model->GetIconAt(i).IsEmpty())
              << "\"" << menu_name << "\" menu item \"" << model->GetLabelAt(i)
              << "\" is missing the icon!";
        }
        if ((menu_type == ui::MenuModel::TYPE_SUBMENU ||
             menu_type == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU) &&
            std::find(skip_commands.cbegin(), skip_commands.cend(),
                      model->GetCommandIdAt(i)) == skip_commands.cend()) {
          check_for_icons_ref(model->GetLabelAt(i), model->GetSubmenuModelAt(i),
                              check_for_icons_ref);
        }
      }
    };
    check_for_icons_impl(menu_name, model, check_for_icons_impl);
  };

  check_for_icons(u"<Root Menu>", &model);
}

class ExtensionsMenuModelTest : public AppMenuModelTest,
                                public testing::WithParamInterface<bool> {
 public:
  ExtensionsMenuModelTest() = default;
  ~ExtensionsMenuModelTest() override = default;

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kExtensionsCollapseMainMenu);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kExtensionsCollapseMainMenu);
    }
    AppMenuModelTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ExtensionsMenuModelTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& param) {
                           return param.param ? "Collapse" : "DoNotCollapse";
                         });

// Tests that extensions sub menu (when enabled) generates the correct elements
// or does not generate its elements when disabled.
TEST_P(ExtensionsMenuModelTest, ExtensionsMenu) {
  AppMenuModel model(this, browser());
  model.Init();

  if (GetParam()) {
    const auto index = model.GetIndexOfCommandId(IDC_FIND_EXTENSIONS);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(nullptr, model.GetSubmenuModelAt(*index));
  } else {
    ASSERT_TRUE(model.GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU));
    ui::MenuModel* extensions_submenu = model.GetSubmenuModelAt(
        model.GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU).value());
    ASSERT_NE(extensions_submenu, nullptr);
    ASSERT_EQ(2ul, extensions_submenu->GetItemCount());
    EXPECT_EQ(IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
              extensions_submenu->GetCommandIdAt(0));
    EXPECT_EQ(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
              extensions_submenu->GetCommandIdAt(1));
  }
}

// Profile row does not show on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
class TestAppMenuModelMetricsTest : public AppMenuModelTest,
                                    public testing::WithParamInterface<int> {
 public:
  TestAppMenuModelMetricsTest() = default;
};

TEST_P(TestAppMenuModelMetricsTest, LogProfileMenuMetrics) {
  int command_id = GetParam();
  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  model.ExecuteCommand(command_id, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TestAppMenuModelMetricsTest,
    testing::Values(IDC_MANAGE_GOOGLE_ACCOUNT,
                    IDC_CLOSE_PROFILE,
                    IDC_CUSTOMIZE_CHROME,
                    IDC_SHOW_SIGNIN_WHEN_PAUSED,
                    IDC_SHOW_SYNC_SETTINGS,
                    IDC_TURN_ON_SYNC,
                    IDC_SHOW_SIGNIN,
                    IDC_OPEN_GUEST_PROFILE,
                    IDC_ADD_NEW_PROFILE,
                    IDC_MANAGE_CHROME_PROFILES,
                    IDC_READING_LIST_MENU_ADD_TAB,
                    IDC_READING_LIST_MENU_SHOW_UI,
                    IDC_SHOW_PASSWORD_MANAGER,
                    IDC_SHOW_PAYMENT_METHODS,
                    IDC_SHOW_ADDRESSES,
                    IDC_SHOW_CONTACT_INFO,
                    IDC_SHOW_IDENTITY_DOCS,
                    IDC_SHOW_TRAVEL,
                    AppMenuModel::kMinOtherProfileCommandId));

TEST_F(AppMenuModelTest, YourSavedInfoSubmenusShown) {
  feature_list_.InitAndEnableFeature(
      autofill::features::kYourSavedInfoSettingsPage);
  AppMenuModel model(this, browser());
  model.Init();

  const size_t your_saved_info_menu_index =
      model.GetIndexOfCommandId(IDC_PASSWORDS_AND_AUTOFILL_MENU).value();
  ui::SimpleMenuModel* your_saved_info_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(your_saved_info_menu_index));

  EXPECT_TRUE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_CONTACT_INFO)
                  .has_value());
  EXPECT_TRUE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_IDENTITY_DOCS)
                  .has_value());
  EXPECT_TRUE(
      your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_TRAVEL).has_value());
}

TEST_F(AppMenuModelTest, YourSavedInfoSubmenusDisabled) {
  feature_list_.InitAndDisableFeature(
      autofill::features::kYourSavedInfoSettingsPage);
  AppMenuModel model(this, browser());
  model.Init();

  const size_t your_saved_info_menu_index =
      model.GetIndexOfCommandId(IDC_PASSWORDS_AND_AUTOFILL_MENU).value();
  ui::SimpleMenuModel* your_saved_info_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(your_saved_info_menu_index));

  EXPECT_FALSE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_CONTACT_INFO)
                   .has_value());
  EXPECT_FALSE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_IDENTITY_DOCS)
                   .has_value());
  EXPECT_FALSE(
      your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_TRAVEL).has_value());
}

TEST_F(AppMenuModelTest, ProfileSyncOnTest) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSync);
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));
  const size_t sync_settings_index =
      profile_menu->GetIndexOfCommandId(IDC_SHOW_SYNC_SETTINGS).value();
  EXPECT_TRUE(profile_menu->IsEnabledAt(sync_settings_index));
}

class AppMenuModelSigninPromoTest : public base::test::WithFeatureOverride,
                                    public AppMenuModelTest {
 public:
  AppMenuModelSigninPromoTest()
      : WithFeatureOverride(syncer::kReplaceSyncPromosWithSignInPromos) {}
  ~AppMenuModelSigninPromoTest() override = default;
};

TEST_P(AppMenuModelSigninPromoTest, SignedIn) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  EXPECT_EQ(!IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_TURN_ON_SYNC).has_value());
  EXPECT_FALSE(profile_menu->GetIndexOfCommandId(IDC_SHOW_SIGNIN).has_value());
}

TEST_P(AppMenuModelSigninPromoTest, SignedOut) {
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  EXPECT_EQ(!IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_TURN_ON_SYNC).has_value());
  EXPECT_EQ(IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_SHOW_SIGNIN).has_value());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AppMenuModelSigninPromoTest);

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
// Tests settings menu items is disabled in the app menu when
// kSystemFeaturesDisableList is set.
TEST_F(AppMenuModelTest, DisableSettingsItem) {
  AppMenuModel model(this, browser());
  model.Init();
  const size_t options_index = model.GetIndexOfCommandId(IDC_OPTIONS).value();
  EXPECT_TRUE(model.IsEnabledAt(options_index));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const size_t help_menu_index =
      model.GetIndexOfCommandId(IDC_HELP_MENU).value();
  ui::SimpleMenuModel* help_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(help_menu_index));
  const size_t about_index = help_menu->GetIndexOfCommandId(IDC_ABOUT).value();
  EXPECT_TRUE(help_menu->IsEnabledAt(about_index));
#else
  const size_t about_index = model.GetIndexOfCommandId(IDC_ABOUT).value();
  EXPECT_TRUE(model.IsEnabledAt(about_index));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kBrowserSettings));
  }
  EXPECT_FALSE(model.IsEnabledAt(options_index));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_FALSE(help_menu->IsEnabledAt(about_index));
#else
  EXPECT_FALSE(model.IsEnabledAt(about_index));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->clear();
  }
  EXPECT_TRUE(model.IsEnabledAt(options_index));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(help_menu->IsEnabledAt(about_index));
#else
  EXPECT_TRUE(model.IsEnabledAt(about_index));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class TestAppMenuModelSafetyHubTest : public AppMenuModelTest {
 public:
  void SetUp() override {
    AppMenuModelTest::SetUp();
    password_store_ = CreateAndUseTestPasswordStore(profile());

    // Let PasswordStatusCheckService run until it fetches the latest data.
    PasswordStatusCheckService* password_service =
        safety_hub_test_util::CreateAndUsePasswordStatusService(profile());

    safety_hub_test_util::UpdatePasswordCheckServiceAsync(password_service);
    EXPECT_EQ(password_service->compromised_credential_count(), 0UL);
  }

 protected:
  scoped_refptr<password_manager::TestPasswordStore> password_store_;
};

TEST_F(TestAppMenuModelSafetyHubTest, SafetyHubMenuNotification) {
  // When there is no issue identified by Safety Hub, there shouldn't be an
  // entry in the AppMenu either.
  AppMenuModel model(this, browser());
  model.Init();
  EXPECT_FALSE(model.GetIndexOfCommandId(IDC_OPEN_SAFETY_HUB).has_value());

  safety_hub_test_util::GenerateSafetyHubMenuNotification(profile());

  AppMenuModel new_model(this, browser());
  new_model.Init();

  // The notification should be shown with the correct label and command.
  EXPECT_TRUE(new_model.GetIndexOfCommandId(IDC_OPEN_SAFETY_HUB).has_value());
  const size_t menu_index =
      new_model.GetIndexOfCommandId(IDC_OPEN_SAFETY_HUB).value();
  new_model.ActivatedAt(menu_index);
  EXPECT_TRUE(new_model.IsEnabledAt(menu_index));
  EXPECT_FALSE(new_model.GetLabelAt(menu_index).empty());
}

class TabSearchMenuModelTest : public AppMenuModelTest {
 public:
  TabSearchMenuModelTest() = default;
  ~TabSearchMenuModelTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kTabstripComboButton,
          {{"tab_search_toolbar_button", "true"}}}},
        /*disabled_features=*/{});
    AppMenuModelTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabSearchMenuModelTest, TabSearchItem) {
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  size_t tab_search_index =
      toolModel.GetIndexOfCommandId(IDC_TAB_SEARCH).value();
  EXPECT_TRUE(toolModel.IsEnabledAt(tab_search_index));
}
