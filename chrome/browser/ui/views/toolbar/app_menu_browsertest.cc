// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/app_menu.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/timer/elapsed_timer.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

namespace {
using testing::AllOf;
using testing::Contains;
using testing::Eq;
using testing::Pointee;
using testing::Property;

class AppMenuBrowserTest : public UiBrowserTest {
 public:
  AppMenuBrowserTest() {
    // Disable the comparison tables submenu.
    // TODO(crbug.com/429347589): Clean up and update test to work by triggering
    // disruptive notification revocation (or other SH feature).
    scoped_feature_list_.InitWithFeatures(
        {}, /*disabled_features=*/{
            features::kSafetyHubDisruptiveNotificationRevocation});
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 protected:
  // Changes the return value of `browser()` as long as the returned object is
  // alive. This resetting behavior is necessary to null `browser_` before its
  // destruction, lest the allocator complain about dangling refs.
  [[nodiscard]] base::AutoReset<raw_ptr<Browser>> SetBrowser(Browser* browser) {
    return base::AutoReset<raw_ptr<Browser>>(&browser_, browser);
  }

  Browser* browser() {
    return browser_ ? browser_.get() : UiBrowserTest::browser();
  }

  BrowserAppMenuButton* menu_button() {
    return views::AsViewClass<BrowserAppMenuButton>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kToolbarAppMenuButtonElementId,
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetElementContext()));
  }

 private:
  raw_ptr<Browser> browser_ = nullptr;
  std::optional<int> command_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void AppMenuBrowserTest::ShowUi(const std::string& name) {
  // Include mnemonics in screenshots so that we detect changes to them.
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);

  if (base::StartsWith(name, "main")) {
    return;
  }

  constexpr auto kSubmenus = base::MakeFixedFlatMap<std::string_view, int>({
      // Submenus present in all versions.
      {"history", AppMenuModel::kRecentTabsMenuPlaceholder},
      {"bookmarks", AppMenuModel::kBookmarksMenuPlaceholder},
      {"bookmarks_comparison_tables", AppMenuModel::kBookmarksMenuPlaceholder},
      {"more_tools", AppMenuModel::kMoreToolsMenuPlaceholder},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"help", AppMenuModel::kHelpMenuPlaceholder},
#endif

      // Submenus only present after Chrome Refresh.
      {"passwords_and_autofill",
       AppMenuModel::kPasswordsAndAutofillMenuPlaceholder},
      {"reading_list",
       AppMenuModel::kReadingListMenuPlaceholder},  // Inside the bookmarks
                                                    // menu.
      {"extensions", AppMenuModel::kExtensionsSubmenuPlaceholder},
      {"find_and_edit", AppMenuModel::kFindAndEditMenuPlaceholder},
      {"save_and_share", AppMenuModel::kSaveAndShareMenuPlaceholder},
      {"profile_menu_in_app_menu_signed_out",
       AppMenuModel::kProfileMenuPlaceholder},
      {"profile_menu_in_app_menu_signed_in",
       AppMenuModel::kProfileMenuPlaceholder},
      {"profile_menu_in_app_menu_signin_not_allowed",
       AppMenuModel::kProfileMenuPlaceholder},
      {"profile_menu_in_app_menu_no_ring",
       AppMenuModel::kProfileMenuPlaceholder},
      {"profile_menu_in_app_menu_single_ring",
       AppMenuModel::kProfileMenuPlaceholder},
      {"profile_menu_in_app_menu_two_consecutive_rings",
       AppMenuModel::kProfileMenuPlaceholder},
  });
  const auto id_entry = kSubmenus.find(name);
  if (id_entry == kSubmenus.end()) {
    ADD_FAILURE() << "Unknown submenu " << name;
    return;
  }
  command_id_ = id_entry->second;
  views::MenuItemView* const menu_root =
      menu_button()->app_menu()->root_menu_item();
  menu_root->GetMenuController()->SelectItemAndOpenSubmenu(
      menu_root->GetMenuItemByID(command_id_.value()));
}

bool AppMenuBrowserTest::VerifyUi() {
  if (!menu_button()->IsMenuShowing()) {
    return false;
  }
  views::MenuItemView* menu_item = menu_button()->app_menu()->root_menu_item();
  if (command_id_.has_value()) {
    menu_item = menu_item->GetMenuItemByID(command_id_.value());
  }
  if (!menu_item->SubmenuIsShowing()) {
    return false;
  }

  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  return VerifyPixelUi(menu_item->GetSubmenu()->GetScrollViewContainer(),
                       test_info->test_suite_name(),
                       test_info->name()) != ui::test::ActionResult::kFailed;
}

void AppMenuBrowserTest::WaitForUserDismissal() {
  base::RunLoop run_loop;

  class CloseWaiter : public AppMenuButtonObserver {
   public:
    explicit CloseWaiter(base::RepeatingClosure quit_closure)
        : quit_closure_(std::move(quit_closure)) {}

    // AppMenuButtonObserver:
    void AppMenuClosed() override { quit_closure_.Run(); }

   private:
    const base::RepeatingClosure quit_closure_;
  } waiter(run_loop.QuitClosure());

  base::ScopedObservation<BrowserAppMenuButton, CloseWaiter> observation(
      &waiter);
  observation.Observe(menu_button());

  run_loop.Run();
}

// This test shows the app-menu with a closed window added to the
// TabRestoreService. This is a regression test to ensure menu code handles this
// properly (this was triggering a crash in AppMenu where it was trying to make
// use of RecentTabsMenuModelDelegate before created). See
// https://crbug.com/40197719 for more.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, ShowWithRecentlyClosedWindow) {
  // Create an additional browser, close it, and ensure it is added to the
  // TabRestoreService.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser()->GetProfile());
  TabRestoreServiceLoadWaiter tab_restore_service_load_waiter(
      tab_restore_service);
  tab_restore_service_load_waiter.Wait();
  Browser* second_browser = CreateBrowser(browser()->GetProfile());
  content::WebContents* new_contents = chrome::AddSelectedTabWithURL(
      second_browser,
      chrome_test_utils::GetTestUrl(
          base::FilePath(), base::FilePath().AppendASCII("simple.html")),
      ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  ui_test_utils::BrowserDestroyedObserver observer(second_browser);
  chrome::CloseWindow(second_browser);
  observer.Wait();
  EXPECT_TRUE(std::ranges::contains(tab_restore_service->entries(),
                                    sessions::tab_restore::Type::WINDOW,
                                    &sessions::tab_restore::Entry::type));

  // Show the AppMenu.
  menu_button()->ShowMenu(views::MenuRunner::NO_FLAGS);
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, ExpandCollapse) {
  EXPECT_FALSE(menu_button()->IsMenuShowing());

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kExpand;
  menu_button()->HandleAccessibleAction(action_data);
  EXPECT_TRUE(menu_button()->IsMenuShowing());
  action_data.action = ax::mojom::Action::kCollapse;
  menu_button()->HandleAccessibleAction(action_data);
  EXPECT_FALSE(menu_button()->IsMenuShowing());
}

// There should be at least one subtest below for every distinct submenu of the
// app menu; note that the "main" menu also counts as a submenu. More tests are
// needed if a submenu can have distinct appearances that should all be tested,
// e.g. if different profile data alters the menu appearance.

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_main) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_main_upgrade_available) {
  UpgradeDetector::GetInstance()->set_upgrade_notification_stage_for_testing(
      UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL);
  UpgradeDetector::GetInstance()->NotifyUpgradeForTesting();
  ShowAndVerifyUi();
}

// TODO(crbug.com/484789570): Flaky on Windows 10 x64 builds.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_InvokeUi_main_guest DISABLED_InvokeUi_main_guest
#else
#define MAYBE_InvokeUi_main_guest InvokeUi_main_guest
#endif
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, MAYBE_InvokeUi_main_guest) {
// TODO(crbug.com/40899974): ChromeOS specific profile logic still needs to be
// updated, setup this test for a Guest user session with appropriate command
// line switches afterwards.
#if !BUILDFLAG(IS_CHROMEOS)
  auto browser_resetter = SetBrowser(CreateGuestBrowser());
  ShowAndVerifyUi();
#endif
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_main_incognito) {
  auto browser_resetter = SetBrowser(CreateIncognitoBrowser());
  ShowAndVerifyUi();
}

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_history) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/520132855): Re-enable test after features::kMenuSimplification
// is fully enabled
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_bookmarks) {
  ShowAndVerifyUi();
}
// Flaky b/40261456
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_more_tools) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, AppMenuViewAccessibleProperties) {
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
  auto* app_menu_view = menu_button()->app_menu()->GetZoomAppMenuViewForTest();
  ui::AXNodeData data;

  ASSERT_TRUE(app_menu_view);
  app_menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kMenu);
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, FullscreenButtonState) {
  menu_button()->ShowMenu(views::MenuRunner::NO_FLAGS);
  views::View& zoom_view =
      CHECK_DEREF(menu_button()->app_menu()->GetZoomAppMenuViewForTest());

  EXPECT_THAT(
      zoom_view.GetChildrenInZOrder(),
      Contains(Pointee(AllOf(
          Property(&views::View::GetTooltipText, Eq(u"Full screen")),
          Property(&views::View::GetViewAccessibility,
                   Property(&views::ViewAccessibility::GetCachedName,
                            ::testing::Truly([](std::u16string_view value) {
                              return value.starts_with(u"Full screen");
                            }))),
          Property(&views::View::GetEnabled, Eq(true))))));

  BrowserView::GetBrowserViewForBrowser(browser())->SetCanFullscreen(false);

  EXPECT_THAT(
      zoom_view.GetChildrenInZOrder(),
      Contains(Pointee(AllOf(
          Property(&views::View::GetTooltipText, Eq(u"Full screen disabled")),
          Property(&views::View::GetViewAccessibility,
                   Property(&views::ViewAccessibility::GetCachedName,
                            ::testing::Truly([](std::u16string_view value) {
                              return value.starts_with(u"Full screen disabled");
                            }))),
          Property(&views::View::GetEnabled, Eq(false))))));
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, FullscreenButtonStateInFullscreen) {
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->IsFullscreen());

  menu_button()->ShowMenu(views::MenuRunner::NO_FLAGS);
  views::View& zoom_view =
      CHECK_DEREF(menu_button()->app_menu()->GetZoomAppMenuViewForTest());

  EXPECT_THAT(
      zoom_view.GetChildrenInZOrder(),
      Contains(Pointee(AllOf(
          Property(&views::View::GetTooltipText, Eq(u"Exit full screen")),
          Property(&views::View::GetViewAccessibility,
                   Property(&views::ViewAccessibility::GetCachedName,
                            ::testing::Truly([](std::u16string_view value) {
                              return value.starts_with(u"Exit full screen");
                            }))),
          Property(&views::View::GetEnabled, Eq(true))))));

  // User should not be trapped in fullscreen mode so button is still enabled
  BrowserView::GetBrowserViewForBrowser(browser())->SetCanFullscreen(false);

  EXPECT_THAT(
      zoom_view.GetChildrenInZOrder(),
      Contains(Pointee(AllOf(
          Property(&views::View::GetTooltipText, Eq(u"Exit full screen")),
          Property(&views::View::GetViewAccessibility,
                   Property(&views::ViewAccessibility::GetCachedName,
                            ::testing::Truly([](std::u16string_view value) {
                              return value.starts_with(u"Exit full screen");
                            }))),
          Property(&views::View::GetEnabled, Eq(true))))));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_help) {
  ShowAndVerifyUi();
}
#endif

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest,
                       DISABLED_InvokeUi_passwords_and_autofill) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_reading_list) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_extensions) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, DISABLED_InvokeUi_find_and_edit) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_save_and_share) {
  ShowAndVerifyUi();
}

#if !BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/520132855): Re-enable test after features::kMenuSimplification
// is fully enabled
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest,
                       DISABLED_InvokeUi_main_profile_signed_in) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  ShowAndVerifyUi();
}

// TODO(crbug.com/375132024): Re-enable test.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest,
                       DISABLED_InvokeUi_profile_menu_in_app_menu_signed_out) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(profile_manager, new_path);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest,
                       InvokeUi_profile_menu_in_app_menu_signed_in) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest,
                       InvokeUi_profile_menu_in_app_menu_signin_not_allowed) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  ShowAndVerifyUi();
}

class AppMenuAiRingBrowserTest : public AppMenuBrowserTest {
 public:
  AppMenuAiRingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        switches::kEnableAiSubscriptionAvatarRing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppMenuAiRingBrowserTest,
                       InvokeUi_profile_menu_in_app_menu_no_ring) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Profiles without rings.
  base::FilePath path1 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params1;
  params1.profile_path = path1;
  params1.profile_name = u"Profile 1";
  storage.AddProfile(std::move(params1));

  base::FilePath path2 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params2;
  params2.profile_path = path2;
  params2.profile_name = u"Profile 2";
  storage.AddProfile(std::move(params2));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuAiRingBrowserTest,
                       InvokeUi_profile_menu_in_app_menu_single_ring) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Profile with ring
  base::FilePath path1 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params1;
  params1.profile_path = path1;
  params1.profile_name = u"Profile 1";
  storage.AddProfile(std::move(params1));
  storage.GetProfileAttributesWithPath(path1)->SetAiSubscriptionTier(1);

  // Profile without ring
  base::FilePath path2 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params2;
  params2.profile_path = path2;
  params2.profile_name = u"Profile 2";
  storage.AddProfile(std::move(params2));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    AppMenuAiRingBrowserTest,
    InvokeUi_profile_menu_in_app_menu_two_consecutive_rings) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Profile with ring
  base::FilePath path1 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params1;
  params1.profile_path = path1;
  params1.profile_name = u"Profile 1";
  storage.AddProfile(std::move(params1));
  storage.GetProfileAttributesWithPath(path1)->SetAiSubscriptionTier(1);

  // Profile with ring
  base::FilePath path2 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params2;
  params2.profile_path = path2;
  params2.profile_name = u"Profile 2";
  storage.AddProfile(std::move(params2));
  storage.GetProfileAttributesWithPath(path2)->SetAiSubscriptionTier(1);

  // Profile without ring
  base::FilePath path3 = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params3;
  params3.profile_path = path3;
  params3.profile_name = u"Profile 3";
  storage.AddProfile(std::move(params3));

  ShowAndVerifyUi();
}

#endif

// Test case for Safety Hub notification.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, Safety_Hub_shown_notification) {
  auto* mock_sentiment_service = static_cast<MockTrustSafetySentimentService*>(
      TrustSafetySentimentServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              browser()->GetProfile(),
              base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  safety_hub_test_util::RunUntilPasswordCheckCompleted(browser()->GetProfile());
  safety_hub_test_util::GenerateSafetyHubMenuNotification(
      browser()->GetProfile());
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
  // Set the elapsed timer of the menu to start 10 seconds ago.
  {
    base::subtle::ScopedTimeClockOverrides override(
        /*time_override=*/
        nullptr,
        /*time_ticks_override=*/
        []() {
          return base::subtle::TimeTicksNowIgnoringOverride() -
                 base::Seconds(10);
        },
        /*thread_ticks_override=*/nullptr);
    menu_button()->SetMenuTimerForTesting(base::ElapsedTimer());
  }
  EXPECT_CALL(
      *mock_sentiment_service,
      TriggerSafetyHubSurvey(
          TrustSafetySentimentService::FeatureArea::kSafetyHubNotification,
          testing::_));
  menu_button()->CloseMenu();
}

#if !BUILDFLAG(IS_CHROMEOS)
class AppMenuProfileGradientRingBrowserTest : public AppMenuBrowserTest {
 public:
  AppMenuProfileGradientRingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        switches::kEnableAiSubscriptionAvatarRing);
  }

  void SetAiSubscriptionTierForProfile(int32_t subscription_tier) {
    browser()->GetProfile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier,
        subscription_tier);
  }

  int GetProfileIconWidth() {
    AppMenu* app_menu = menu_button()->app_menu();
    CHECK(app_menu);
    views::MenuItemView* menu_root = app_menu->root_menu_item();
    CHECK(menu_root);
    views::MenuItemView* profile_item =
        menu_root->GetMenuItemByID(AppMenuModel::kProfileMenuPlaceholder);
    CHECK(profile_item);
    ui::ImageModel icon = profile_item->GetIcon();
    CHECK(!icon.IsEmpty());
    return icon.Size().width();
  }

  void CloseMenuAndWait() {
    AppMenu* app_menu = menu_button()->app_menu();
    ASSERT_TRUE(app_menu);
    views::MenuItemView* menu_root = app_menu->root_menu_item();
    ASSERT_TRUE(menu_root);
    views::SubmenuView* submenu = menu_root->GetSubmenu();
    ASSERT_TRUE(submenu);
    views::Widget* menu_widget = submenu->GetWidget();
    ASSERT_TRUE(menu_widget);

    views::test::WidgetDestroyedWaiter waiter(menu_widget);
    menu_button()->CloseMenu();
    waiter.Wait();
    ASSERT_FALSE(menu_button()->IsMenuShowing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppMenuProfileGradientRingBrowserTest,
                       ProfileMenuIconHasGradientRing) {
  // Sign in with an image to get a non-placeholder avatar.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "user@example.com", signin::ConsentLevel::kSignin);

  // Simulate account image fetch.
  gfx::Image fake_image = gfx::test::CreateImage(20, 20, SK_ColorBLUE);
  signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                    "http://example.com/avatar.jpg",
                                    fake_image);

  // 1. Get initial size (no subscription).
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
  ASSERT_TRUE(menu_button()->IsMenuShowing());
  int initial_size = GetProfileIconWidth();
  ASSERT_GT(initial_size, 0);
  CloseMenuAndWait();

  // 2. Set AI subscription and check size increases.
  SetAiSubscriptionTierForProfile(1);
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
  ASSERT_TRUE(menu_button()->IsMenuShowing());
  int size_with_ring = GetProfileIconWidth();
  EXPECT_GT(size_with_ring, initial_size);
  CloseMenuAndWait();

  // 3. Clear subscription and check size goes back to initial.
  SetAiSubscriptionTierForProfile(0);
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
  ASSERT_TRUE(menu_button()->IsMenuShowing());
  int final_size = GetProfileIconWidth();
  EXPECT_EQ(final_size, initial_size);
  CloseMenuAndWait();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
