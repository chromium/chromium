// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

class TabSearchBubbleHostBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchBubbleHostBrowserTest() {
    // TODO(crbug.com/584313535): Delete old tab search tests after launch.
    scoped_feature_list_.InitAndDisableFeature(
        organizer_panel::kOrganizerPanel);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchBubbleHost* tab_search_bubble_host() {
    return browser_view()->GetTabSearchBubbleHost();
  }

  WebUIBubbleManager* bubble_manager() {
    return tab_search_bubble_host()->webui_bubble_manager_for_testing();
  }

  void RunUntilBubbleWidgetDestroyed() {
    RunUntilBubbleWidgetDestroyed(bubble_manager());
  }

  void RunUntilBubbleWidgetDestroyed(WebUIBubbleManager* manager) {
    ASSERT_NE(nullptr, manager->GetBubbleWidget());
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_EQ(nullptr, manager->GetBubbleWidget());
  }

 private:
#if BUILDFLAG(IS_MAC)
  // Fake the macOS fullscreen transition. browser_tests run in parallel and
  // real AppKit fullscreen Space transitions are flaky there.
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen_;
#endif
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleShowTimerTriggersCorrectly) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  tab_search_bubble_host()->ShowTabSearchBubble();

  // |bubble_created_time_| should be set as soon as the bubble widget is
  // created.
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(tab_search_bubble_host()->bubble_created_time_for_testing());

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  // Closing the bubble should reset the timestamp.
  tab_search_bubble_host()->CloseTabSearchBubble();
  EXPECT_FALSE(tab_search_bubble_host()->bubble_created_time_for_testing());
  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleShowCorrectlyInFullscreen) {
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  gfx::Rect rect(20, 4, 0, 0);
  bubble_manager()->ShowBubble(rect);

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  gfx::Rect bound =
      bubble_manager()->bubble_view_for_testing()->GetAnchorRect();
  EXPECT_EQ(bound, rect);

  tab_search_bubble_host()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleBackgroundColorMatchesBrowserTheme) {
  // Set up browser theme
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  test::ThemeServiceChangedWaiter waiter(theme_service);
  theme_service->SetUserColor(SK_ColorGREEN);
  waiter.WaitForThemeChanged();

  // Force preload Tab Search immediately using the Test API
  WebUIContentsPreloadManagerTestAPI test_api;
  test_api.PreloadUrl(browser()->GetProfile(),
                      GURL(chrome::kChromeUITabSearchURL));

  auto* preload_manager = WebUIContentsPreloadManager::GetInstance();
  SkColor expected_color =
      browser_view()->GetColorProvider()->GetColor(ui::kColorBubbleBackground);

  // Verify that the preloaded WebContents gets the correct theme color
  // in the background *before* the bubble is shown on screen.
  content::WebContents* preloaded_contents =
      preload_manager->preloaded_web_contents();
  ASSERT_TRUE(preloaded_contents);
  EXPECT_EQ(preloaded_contents->GetColorProvider().GetColor(
                ui::kColorBubbleBackground),
            expected_color);
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       KeyboardShortcutTriggersBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());

  auto accelerator = ui::Accelerator(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  browser_view()->AcceleratorPressed(accelerator);

  // Accelerator keys should have created the tab search bubble.
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_bubble_host()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}
#endif

class FullscreenTabSearchBubbleDialogTest : public DialogBrowserTest {
 public:
  FullscreenTabSearchBubbleDialogTest() {
    // TODO(crbug.com/584313535): Delete old tab search tests after launch.
    scoped_feature_list_.InitAndDisableFeature(
        organizer_panel::kOrganizerPanel);
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);
#endif
  }

  FullscreenTabSearchBubbleDialogTest(
      const FullscreenTabSearchBubbleDialogTest&) = delete;
  FullscreenTabSearchBubbleDialogTest& operator=(
      const FullscreenTabSearchBubbleDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    ui_test_utils::ToggleFullscreenModeAndWait(browser());
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser());
    view->CreateTabSearchBubble();
  }

 private:
#if BUILDFLAG(IS_MAC)
  // Fake the macOS fullscreen transition. browser_tests run in parallel and
  // real AppKit fullscreen Space transitions are flaky there.
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen_;
#endif
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FullscreenTabSearchBubbleDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

class TabSearchBubbleHostSharedRendererBrowserTest
    : public TabSearchBubbleHostBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kTopChromeWebUISharedRenderer};
};

// With kTopChromeWebUISharedRenderer, tab search is preloaded in
// TopChromeWebUIProfile for the profile of the window, and the bubble shows
// the preloaded WebContents.
IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostSharedRendererBrowserTest,
                       PreloadsAndShowsInTopChromeWebUIProfile) {
  Profile* const user_profile = browser()->GetProfile();
  WebUIContentsPreloadManagerTestAPI test_api;
  test_api.PreloadUrl(user_profile, GURL(chrome::kChromeUITabSearchURL));
  content::WebContents* preloaded =
      WebUIContentsPreloadManager::GetInstance()->preloaded_web_contents();
  ASSERT_TRUE(preloaded);
  EXPECT_TRUE(Profile::FromBrowserContext(preloaded->GetBrowserContext())
                  ->IsTopChromeWebUIProfile());
  EXPECT_EQ(user_profile, webui::GetUserProfile(preloaded));

  tab_search_bubble_host()->ShowTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->bubble_view_for_testing());
  content::WebContents* shown = bubble_manager()
                                    ->bubble_view_for_testing()
                                    ->get_contents_wrapper_for_testing()
                                    ->web_contents();
  EXPECT_EQ(preloaded, shown);
  EXPECT_TRUE(content::WaitForLoadStop(shown));
  // The page handlers use the user profile.
  EXPECT_TRUE(content::ExecJs(shown, "true"));

  tab_search_bubble_host()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

// With kTopChromeWebUISharedRenderer, tab search isn't preloaded for an
// Incognito window, but its bubble is still created in TopChromeWebUIProfile
// for the Incognito profile.
IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostSharedRendererBrowserTest,
                       ShowsInTopChromeWebUIProfileWithoutPreloadForIncognito) {
  BrowserWindowInterface* const incognito_browser = CreateIncognitoBrowser();
  Profile* const incognito_profile = incognito_browser->GetProfile();
  // Smart preloading would preload an engaged WebUI in a regular profile.
  site_engagement::SiteEngagementService::Get(incognito_profile)
      ->ResetBaseScoreForURL(GURL(chrome::kChromeUITabSearchURL), 50);
  WebUIContentsPreloadManagerTestAPI test_api;
  test_api.MaybePreloadForBrowserContext(incognito_profile);
  content::WebContents* preloaded =
      WebUIContentsPreloadManager::GetInstance()->preloaded_web_contents();
  EXPECT_FALSE(preloaded &&
               webui::GetUserProfile(preloaded) == incognito_profile);

  TabSearchBubbleHost* host =
      BrowserView::GetBrowserViewForBrowser(incognito_browser)
          ->GetTabSearchBubbleHost();
  host->ShowTabSearchBubble();
  WebUIBubbleManager* manager = host->webui_bubble_manager_for_testing();
  ASSERT_TRUE(manager->bubble_view_for_testing());
  content::WebContents* shown = manager->bubble_view_for_testing()
                                    ->get_contents_wrapper_for_testing()
                                    ->web_contents();
  EXPECT_TRUE(Profile::FromBrowserContext(shown->GetBrowserContext())
                  ->IsTopChromeWebUIProfile());
  EXPECT_EQ(incognito_profile, webui::GetUserProfile(shown));
  EXPECT_TRUE(content::WaitForLoadStop(shown));
  EXPECT_TRUE(content::ExecJs(shown, "true"));
  host->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed(manager);
}
