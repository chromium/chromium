// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chromeos/constants/chromeos_features.h"
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
  void ExecuteMenuItem(BrowserWindowInterface* browser) override {
    execute_count_++;
  }

  bool HasBubbleView() override { return false; }
  bool HasShownBubbleView() override { return false; }
  void ShowBubbleView(BrowserWindowInterface* browser) override {
    ADD_FAILURE();
  }
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

class AppMenuModelTest : public InProcessBrowserTest,
                         public ui::AcceleratorProvider {
 public:
  AppMenuModelTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    safety_hub_test_util::CreateRevokedPermissionsService(
        browser()->GetProfile());
    safety_hub_test_util::CreateNotificationPermissionsReviewService(
        browser()->GetProfile());
  }

  AppMenuModelTest(const AppMenuModelTest&) = delete;
  AppMenuModelTest& operator=(const AppMenuModelTest&) = delete;

  ~AppMenuModelTest() override = default;

  Profile* profile() { return browser()->GetProfile(); }

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

  void ExecuteCommand(int command_id, int event_flags) override {
    LogMenuMetrics(command_id);
  }

  void LogMenuAction(AppMenuAction action_id) override { log_metrics_count_++; }

  int log_metrics_count_ = 0;
};

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, Basics) {
  // Simulate that an update is available to ensure that the menu includes the
  // upgrade item for platforms that support it.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  detector->set_upgrade_notification_stage(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  detector->NotifyUpgrade();
  EXPECT_TRUE(detector->notify_upgrade());

  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->GetProfile(),
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
      model.GetIndexOfCommandId(AppMenuModel::kBookmarksMenuPlaceholder)
          .value();

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
IN_PROC_BROWSER_TEST_F(AppMenuModelTest, GlobalError) {
  // Make sure services required for tests are initialized.
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->GetProfile());
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
IN_PROC_BROWSER_TEST_F(AppMenuModelTest, DefaultBrowserPrompt) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->GetProfile(),
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

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, PerformanceItem) {
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  ASSERT_TRUE(toolModel.GetIndexOfCommandId(IDC_PERFORMANCE));
  size_t performance_index =
      toolModel.GetIndexOfCommandId(IDC_PERFORMANCE).value();
  EXPECT_TRUE(toolModel.IsEnabledAt(performance_index));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, CustomizeChromeItem) {
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

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, CustomizeChromeLogMetrics) {
  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  model.ExecuteCommand(IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}

class AppMenuModelGlicTest : public AppMenuModelTest {
 public:
  AppMenuModelGlicTest() {
    feature_list_.InitWithFeatures({features::kGlic, features::kGlicRollout},
                                   {});
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuModelGlicTest, GlicItem) {
  TestLogMetricsAppMenuModel model(this, browser());
  model.Init();
  model.ExecuteCommand(IDC_OPEN_GLIC, 0);
  EXPECT_EQ(1, model.log_metrics_count_);
}

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, DoNotShowShareSubMenuItem) {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
#if !BUILDFLAG(IS_CHROMEOS)
  prefs->SetBoolean(prefs::kDesktopSharingHubEnabled, false);
#endif
  prefs->SetBoolean(prefs::kDisableScreenshots, true);

  AppMenuModel model(this, browser());
  model.Init();

  ASSERT_TRUE(
      model.GetIndexOfCommandId(AppMenuModel::kSaveAndShareMenuPlaceholder));
  ui::MenuModel* submenu = model.GetSubmenuModelAt(
      model.GetIndexOfCommandId(AppMenuModel::kSaveAndShareMenuPlaceholder)
          .value());
  ASSERT_NE(submenu, nullptr);

  size_t expected_item_count = 7;
  if (!sharing_hub::SharingIsDisabledByPolicy(browser()->GetProfile()) ||
      sharing_hub::DesktopScreenshotsFeatureEnabled(browser()->GetProfile())) {
    expected_item_count += 2;
    if (!sharing_hub::SharingIsDisabledByPolicy(browser()->GetProfile())) {
      // Copy URL, Send Tab to Self, and QR code generator items are always
      // included when sharing is enabled by policy.
      expected_item_count += 3;
    }
    if (sharing_hub::DesktopScreenshotsFeatureEnabled(
            browser()->GetProfile())) {
      expected_item_count += 1;
    }
  }
  EXPECT_EQ(expected_item_count, submenu->GetItemCount());
}

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, ModelHasIcons) {
  // Skip the items that are either not supposed to have an icon, or are not
  // ready to be tested. Remove items once they're ready for testing.
  const std::vector<int> skip_commands = {
      kRecentTabsNoDeviceTabsId,
      IDC_ABOUT,
      RecentTabsSubMenuModel::GetDisabledRecentlyClosedHeaderCommandId(),
      IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
      IDC_TAKE_SCREENSHOT,
      IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW,
      IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE,
      IDC_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP};
  AppMenuModel model(this, browser());
  model.Init();

  const auto check_for_icons = [&skip_commands](std::u16string menu_name,
                                                ui::MenuModel* model) -> void {
    auto check_for_icons_impl =
        [&skip_commands](std::u16string menu_name, ui::MenuModel* model,
                         auto& check_for_icons_ref) -> void {
      // Except where noted by the above vector, all menu items in CR2023 must
      // have icons.
      for (size_t i = 0; i < model->GetItemCount(); ++i) {
        auto menu_type = model->GetTypeAt(i);
        if (menu_type != ui::MenuModel::TYPE_ACTIONABLE_SUBMENU &&
            menu_type != ui::MenuModel::TYPE_SUBMENU &&
            std::ranges::contains(skip_commands, model->GetCommandIdAt(i))) {
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
            !std::ranges::contains(skip_commands, model->GetCommandIdAt(i))) {
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
  ExtensionsMenuModelTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kExtensionsCollapseMainMenu);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kExtensionsCollapseMainMenu);
    }
  }
  ~ExtensionsMenuModelTest() override = default;

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
IN_PROC_BROWSER_TEST_P(ExtensionsMenuModelTest, ExtensionsMenu) {
  AppMenuModel model(this, browser());
  model.Init();

  if (GetParam()) {
    const auto index = model.GetIndexOfCommandId(IDC_FIND_EXTENSIONS);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(nullptr, model.GetSubmenuModelAt(*index));
  } else {
    ASSERT_TRUE(
        model.GetIndexOfCommandId(AppMenuModel::kExtensionsSubmenuPlaceholder));
    ui::MenuModel* extensions_submenu = model.GetSubmenuModelAt(
        model.GetIndexOfCommandId(AppMenuModel::kExtensionsSubmenuPlaceholder)
            .value());
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

  void SetUpOnMainThread() override {
    AppMenuModelTest::SetUpOnMainThread();
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "user@example.com", signin::ConsentLevel::kSync);
  }
};

IN_PROC_BROWSER_TEST_P(TestAppMenuModelMetricsTest, LogProfileMenuMetrics) {
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
                    IDC_SHOW_CONTACT_INFO,
                    IDC_SHOW_IDENTITY_DOCS,
                    IDC_SHOW_TRAVEL,
                    AppMenuModel::kMinOtherProfileCommandId));

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, YourSavedInfoSubmenusShown) {
  AppMenuModel model(this, browser());
  model.Init();

  const size_t your_saved_info_menu_index =
      model
          .GetIndexOfCommandId(
              AppMenuModel::kPasswordsAndAutofillMenuPlaceholder)
          .value();
  ui::SimpleMenuModel* your_saved_info_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(your_saved_info_menu_index));

  EXPECT_TRUE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_CONTACT_INFO)
                  .has_value());
  EXPECT_TRUE(your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_IDENTITY_DOCS)
                  .has_value());
  EXPECT_TRUE(
      your_saved_info_menu->GetIndexOfCommandId(IDC_SHOW_TRAVEL).has_value());
}

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, ProfileSyncOnTest) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSync);
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kProfileMenuPlaceholder).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));
  const size_t sync_settings_index =
      profile_menu->GetIndexOfCommandId(IDC_SHOW_SYNC_SETTINGS).value();
  EXPECT_TRUE(profile_menu->IsEnabledAt(sync_settings_index));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
bool DoesHelpMenuHaveCommand(const AppMenuModel& model, int command_id) {
  const size_t help_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kHelpMenuPlaceholder).value();
  ui::SimpleMenuModel* help_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(help_menu_index));
  return help_menu->GetIndexOfCommandId(command_id).has_value();
}

IN_PROC_BROWSER_TEST_F(AppMenuModelTest, Feedback_UserFeedbackAllowedPolicy) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  true);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_TRUE(DoesHelpMenuHaveCommand(model, IDC_FEEDBACK));
  }

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  false);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_FALSE(DoesHelpMenuHaveCommand(model, IDC_FEEDBACK));
  }
}

class AppMenuReportUnsafeSiteTest : public base::test::WithFeatureOverride,
                                    public AppMenuModelTest {
 public:
  AppMenuReportUnsafeSiteTest()
      : WithFeatureOverride(features::kReportUnsafeSite) {}
  ~AppMenuReportUnsafeSiteTest() override = default;
};

IN_PROC_BROWSER_TEST_P(AppMenuReportUnsafeSiteTest,
                       ReportUnsafeSite_UserFeedbackAllowedPolicy) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  true);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_EQ(IsParamFeatureEnabled(),
              DoesHelpMenuHaveCommand(model, IDC_REPORT_UNSAFE_SITE));
  }

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  false);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_FALSE(DoesHelpMenuHaveCommand(model, IDC_REPORT_UNSAFE_SITE));
  }
}

IN_PROC_BROWSER_TEST_P(AppMenuReportUnsafeSiteTest,
                       ReportUnsafeSite_SafeBrowsingDisabled) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                  true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                  true);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_EQ(IsParamFeatureEnabled(),
              DoesHelpMenuHaveCommand(model, IDC_REPORT_UNSAFE_SITE));
  }

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                  false);
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_FALSE(DoesHelpMenuHaveCommand(model, IDC_REPORT_UNSAFE_SITE));
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AppMenuReportUnsafeSiteTest);

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class AppMenuModelSigninPromoTest : public base::test::WithFeatureOverride,
                                    public AppMenuModelTest {
 public:
  AppMenuModelSigninPromoTest()
      : WithFeatureOverride(syncer::kReplaceSyncPromosWithSignInPromos) {
    scoped_feature_list_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
        IsParamFeatureEnabled());
  }
  ~AppMenuModelSigninPromoTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppMenuModelSigninPromoTest, SignedIn) {
  base::HistogramTester histogram_tester;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kProfileMenuPlaceholder).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  EXPECT_EQ(!IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_TURN_ON_SYNC).has_value());
  EXPECT_FALSE(profile_menu->GetIndexOfCommandId(IDC_SHOW_SIGNIN).has_value());

  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
}

IN_PROC_BROWSER_TEST_P(AppMenuModelSigninPromoTest, SignedOut) {
  base::HistogramTester histogram_tester;
  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kProfileMenuPlaceholder).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  EXPECT_EQ(!IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_TURN_ON_SYNC).has_value());
  EXPECT_EQ(IsParamFeatureEnabled(),
            profile_menu->GetIndexOfCommandId(IDC_SHOW_SIGNIN).has_value());

  if (IsParamFeatureEnabled()) {
    histogram_tester.ExpectUniqueSample("Signin.SignIn.Offered",
                                        signin_metrics::AccessPoint::kMenu, 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.SignIn.Offered.NewAccountNoExistingAccount",
        signin_metrics::AccessPoint::kMenu, 1);
  } else {
    histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AppMenuModelSigninPromoTest);

class AppMenuModelBookmarkLimitExceededSyncingTest : public AppMenuModelTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AppMenuModelTest::SetUpBrowserContextKeyedServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<syncer::TestSyncService>();
          service->SetBookmarksLimitExceeded(true);
          return service;
        }));
  }
};

IN_PROC_BROWSER_TEST_F(
    AppMenuModelBookmarkLimitExceededSyncingTest,
    ProfileSyncBookmarkLimitExceededErrorHiddenTest_Syncing) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSync);

  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kProfileMenuPlaceholder).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  // Verify that IDC_SHOW_SYNC_SETTINGS is NOT present because we returned true
  // early.
  EXPECT_FALSE(
      profile_menu->GetIndexOfCommandId(IDC_SHOW_SYNC_SETTINGS).has_value());
  // Verify that the "Learn more" button (which would have command_id 0) is NOT
  // present.
  EXPECT_FALSE(profile_menu->GetIndexOfCommandId(0).has_value());
}

class AppMenuModelBookmarkLimitExceededSignedInNonSyncingTest
    : public AppMenuModelTest {
 public:
  AppMenuModelBookmarkLimitExceededSignedInNonSyncingTest() {
    feature_list_.InitAndEnableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AppMenuModelTest::SetUpBrowserContextKeyedServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<syncer::TestSyncService>();
          service->SetBookmarksLimitExceeded(true);
          return service;
        }));
  }
};

IN_PROC_BROWSER_TEST_F(
    AppMenuModelBookmarkLimitExceededSignedInNonSyncingTest,
    ProfileSyncBookmarkLimitExceededErrorHiddenTest_SignedInNonSyncing) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);

  AppMenuModel model(this, browser());
  model.Init();
  const size_t profile_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kProfileMenuPlaceholder).value();
  ui::SimpleMenuModel* profile_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(profile_menu_index));

  // Verify that IDC_SHOW_SYNC_SETTINGS is NOT present because we returned true
  // early.
  EXPECT_FALSE(
      profile_menu->GetIndexOfCommandId(IDC_SHOW_SYNC_SETTINGS).has_value());
  // Verify that the "Learn more" button (which would have command_id 0) is NOT
  // present.
  EXPECT_FALSE(profile_menu->GetIndexOfCommandId(0).has_value());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
// Tests settings menu items is disabled in the app menu when
// kSystemFeaturesDisableList is set.
IN_PROC_BROWSER_TEST_F(AppMenuModelTest, DisableSettingsItem) {
  AppMenuModel model(this, browser());
  model.Init();
  const size_t options_index = model.GetIndexOfCommandId(IDC_OPTIONS).value();
  EXPECT_TRUE(model.IsEnabledAt(options_index));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const size_t help_menu_index =
      model.GetIndexOfCommandId(AppMenuModel::kHelpMenuPlaceholder).value();
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
        g_browser_process->local_state(),
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
        g_browser_process->local_state(),
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
  TestAppMenuModelSafetyHubTest() {
    // Disruptive notification revocation disables the notification review
    // module.
    // TODO(https://crbug.com/496616827): Clean up this test when removing the
    // notification review module logic.
    scoped_feature_list_.InitAndDisableFeature(
        features::kSafetyHubDisruptiveNotificationRevocation);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AppMenuModelTest::SetUpBrowserContextKeyedServices(context);
    CreateAndUseTestPasswordStore(context);
  }

  void SetUpOnMainThread() override {
    AppMenuModelTest::SetUpOnMainThread();

    // Let PasswordStatusCheckService run until it fetches the latest data.
    PasswordStatusCheckService* password_service =
        safety_hub_test_util::CreateAndUsePasswordStatusService(profile());

    safety_hub_test_util::UpdatePasswordCheckServiceAsync(password_service);
    EXPECT_EQ(password_service->compromised_credential_count(), 0UL);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TestAppMenuModelSafetyHubTest,
                       SafetyHubMenuNotification) {
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
  TabSearchMenuModelTest() {
    glic_enabled_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{features::kGlicLocaleFiltering,
                               features::kGlicCountryFiltering});
  }

  ~TabSearchMenuModelTest() override = default;

  void SetUpOnMainThread() override {
    AppMenuModelTest::SetUpOnMainThread();
    // This is necessary because the global features that GlicEnabling depends
    // on are not initialized for glic.
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }

  void TearDownOnMainThread() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
    AppMenuModelTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList glic_enabled_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchMenuModelTest, TabSearchItem) {
  AppMenuModel model(this, browser());
  model.Init();
  ToolsMenuModel toolModel(&model, browser());
  std::optional<size_t> tab_search_index =
      toolModel.GetIndexOfCommandId(IDC_TAB_SEARCH);
  EXPECT_TRUE(tab_search_index.has_value());
  EXPECT_TRUE(toolModel.IsEnabledAt(tab_search_index.value()));
}

class AppMenuModelBookmarkBarTest : public AppMenuModelTest,
                                    public testing::WithParamInterface<bool> {
 public:
  AppMenuModelBookmarkBarTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ntp_features::kNtpSimplificationBookmarkBar);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ntp_features::kNtpSimplificationBookmarkBar);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppMenuModelBookmarkBarTest, BookmarkBarSubmenu) {
  AppMenuModel model(this, browser());
  model.Init();
  BookmarkSubMenuModel bookmark_sub_model(&model, browser());

  if (GetParam()) {
    // Feature enabled: should have a submenu for Bookmarks Bar.
    EXPECT_TRUE(bookmark_sub_model.GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU)
                    .has_value());
    EXPECT_FALSE(bookmark_sub_model.GetIndexOfCommandId(IDC_SHOW_BOOKMARK_BAR)
                     .has_value());

    // Check items inside the submenu model.
    auto index =
        bookmark_sub_model.GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU);
    ASSERT_TRUE(index.has_value());
    ui::SimpleMenuModel* sub_model = static_cast<ui::SimpleMenuModel*>(
        bookmark_sub_model.GetSubmenuModelAt(index.value()));
    ASSERT_TRUE(sub_model);

    EXPECT_TRUE(
        sub_model->GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW)
            .has_value());
    EXPECT_TRUE(
        sub_model->GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE)
            .has_value());
    EXPECT_TRUE(
        sub_model->GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP)
            .has_value());
  } else {
    // Feature disabled: should have a single toggle item for Bookmarks Bar.
    EXPECT_FALSE(
        bookmark_sub_model.GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU)
            .has_value());
    EXPECT_TRUE(bookmark_sub_model.GetIndexOfCommandId(IDC_SHOW_BOOKMARK_BAR)
                    .has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(All, AppMenuModelBookmarkBarTest, testing::Bool());

class AppMenuModelEnterpriseReleaseNotesTest
    : public base::test::WithFeatureOverride,
      public AppMenuModelTest {
 public:
  AppMenuModelEnterpriseReleaseNotesTest()
      : WithFeatureOverride(features::kEnterpriseReleaseNotes) {}
  ~AppMenuModelEnterpriseReleaseNotesTest() override = default;
};

IN_PROC_BROWSER_TEST_P(AppMenuModelEnterpriseReleaseNotesTest, MenuVisibility) {
  {
    AppMenuModel model(this, browser());
    model.Init();
    EXPECT_FALSE(model.GetIndexOfCommandId(IDC_CHROME_ENTERPRISE_RELEASE_NOTES)
                     .has_value());
  }

  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  {
    AppMenuModel model(this, browser());
    model.Init();
#if BUILDFLAG(IS_LINUX)
    EXPECT_EQ(IsParamFeatureEnabled(),
              model.GetIndexOfCommandId(IDC_CHROME_ENTERPRISE_RELEASE_NOTES)
                  .has_value());
#else
    EXPECT_FALSE(model.GetIndexOfCommandId(IDC_CHROME_ENTERPRISE_RELEASE_NOTES)
                     .has_value());
#endif
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AppMenuModelEnterpriseReleaseNotesTest);

namespace {

using send_tab_to_self::EntryPointDisplayReason;
using send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2;
using send_tab_to_self::StubSendTabToSelfSyncService;

class AppMenuModelSendTabToSelfTest : public AppMenuModelTest {
 public:
  AppMenuModelSendTabToSelfTest() = default;
  ~AppMenuModelSendTabToSelfTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AppMenuModelTest::SetUpBrowserContextKeyedServices(context);
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }
};

class AppMenuModelSendTabToSelfEnhancedEnabledTest
    : public AppMenuModelSendTabToSelfTest {
 public:
  AppMenuModelSendTabToSelfEnhancedEnabledTest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUIv2);
  }
};

class AppMenuModelSendTabToSelfEnhancedDisabledTest
    : public AppMenuModelSendTabToSelfTest {
 public:
  AppMenuModelSendTabToSelfEnhancedDisabledTest() {
    feature_list_.InitAndDisableFeature(kSendTabToSelfEnhancedDesktopUIv2);
  }
};

// Tests that when kSendTabToSelfEnhancedDesktopUIv2 feature is enabled, the
// "Send to Your Devices" item in the Save and Share submenu is a submenu model.
IN_PROC_BROWSER_TEST_F(AppMenuModelSendTabToSelfEnhancedEnabledTest,
                       SendTabToSelfSaveAndShareSubmenuEnabled) {
  auto* sync_service = static_cast<StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  sync_service->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  AppMenuModel model(this, browser());
  model.Init();

  const size_t save_and_share_index =
      model.GetIndexOfCommandId(AppMenuModel::kSaveAndShareMenuPlaceholder)
          .value();
  ui::SimpleMenuModel* save_and_share_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(save_and_share_index));

  const size_t send_tab_index =
      save_and_share_menu->GetIndexOfCommandId(IDC_SEND_TAB_TO_SELF).value();
  EXPECT_EQ(ui::MenuModel::TYPE_SUBMENU,
            save_and_share_menu->GetTypeAt(send_tab_index));
  EXPECT_NE(nullptr, save_and_share_menu->GetSubmenuModelAt(send_tab_index));
}

// Tests that when kSendTabToSelfEnhancedDesktopUIv2 feature is disabled, the
// "Send to Your Devices" item in the Save and Share submenu remains a simple
// command.
IN_PROC_BROWSER_TEST_F(AppMenuModelSendTabToSelfEnhancedDisabledTest,
                       SendTabToSelfSaveAndShareSubmenuDisabled) {
  auto* sync_service = static_cast<StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  sync_service->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  AppMenuModel model(this, browser());
  model.Init();

  const size_t save_and_share_index =
      model.GetIndexOfCommandId(AppMenuModel::kSaveAndShareMenuPlaceholder)
          .value();
  ui::SimpleMenuModel* save_and_share_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(save_and_share_index));

  const size_t send_tab_index =
      save_and_share_menu->GetIndexOfCommandId(IDC_SEND_TAB_TO_SELF).value();
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND,
            save_and_share_menu->GetTypeAt(send_tab_index));
}

// Tests that when Send Tab to Self is not offered for the active page,
// the item is still present in the Save and Share submenu as a fallback command
// item.
IN_PROC_BROWSER_TEST_F(AppMenuModelSendTabToSelfEnhancedEnabledTest,
                       SendTabToSelfSaveAndShareNotOffered) {
  auto* sync_service = static_cast<StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  sync_service->SetEntryPointDisplayReason(std::nullopt);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  AppMenuModel model(this, browser());
  model.Init();

  const size_t save_and_share_index =
      model.GetIndexOfCommandId(AppMenuModel::kSaveAndShareMenuPlaceholder)
          .value();
  ui::SimpleMenuModel* save_and_share_menu = static_cast<ui::SimpleMenuModel*>(
      model.GetSubmenuModelAt(save_and_share_index));

  const std::optional<size_t> send_tab_index =
      save_and_share_menu->GetIndexOfCommandId(IDC_SEND_TAB_TO_SELF);
  ASSERT_TRUE(send_tab_index.has_value());
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND,
            save_and_share_menu->GetTypeAt(send_tab_index.value()));
  EXPECT_FALSE(save_and_share_menu->IsEnabledAt(send_tab_index.value()));
}

}  // namespace
