// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/blocked_content/list_item_position.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/switches.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "third_party/blink/public/common/switches.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

using content::WebContents;
using testing::_;
using testing::Return;

namespace {

class CloseObserver : public content::WebContentsObserver {
 public:
  explicit CloseObserver(WebContents* contents)
      : content::WebContentsObserver(contents) {}

  CloseObserver(const CloseObserver&) = delete;
  CloseObserver& operator=(const CloseObserver&) = delete;

  void Wait() { close_loop_.Run(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { close_loop_.Quit(); }

 private:
  base::RunLoop close_loop_;
};

class PopupBlockerBrowserTest : public InProcessBrowserTest {
 public:
  PopupBlockerBrowserTest() {}

  PopupBlockerBrowserTest(const PopupBlockerBrowserTest&) = delete;
  PopupBlockerBrowserTest& operator=(const PopupBlockerBrowserTest&) = delete;

  ~PopupBlockerBrowserTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Testing on some platforms is flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }
#endif

  int GetBlockedContentsCount() {
    // Do a round trip to the renderer first to flush any in-flight IPCs to
    // create a to-be-blocked window.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    if (!content::ExecJs(tab, std::string(),
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)) {
      ADD_FAILURE() << "Failed to execute script in active tab.";
      return -1;
    }
    blocked_content::PopupBlockerTabHelper* popup_blocker_helper =
        blocked_content::PopupBlockerTabHelper::FromWebContents(tab);
    return popup_blocker_helper->GetBlockedPopupsCount();
  }

  enum WhatToExpect {
    kExpectPopup,
    kExpectForegroundTab,
    kExpectBackgroundTab,
    kExpectNewWindow
  };

  enum ShouldCheckTitle { kCheckTitle, kDontCheckTitle };

  void NavigateAndCheckPopupShown(const GURL& url,
                                  WhatToExpect what_to_expect) {
    ui_test_utils::TabAddedWaiter tab_added(browser());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    if (what_to_expect == kExpectPopup) {
      ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    } else {
      tab_added.Wait();
      ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
      ASSERT_EQ(2, browser()->tab_strip_model()->count());

      // Check that we always create foreground tabs.
      ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
    }

    ASSERT_EQ(0, GetBlockedContentsCount());
  }

  // Navigates to the test indicated by |test_name| using |browser| which is
  // expected to try to open a popup. Verifies that the popup was blocked and
  // then opens the blocked popup. Once the popup stopped loading, verifies
  // that the title of the page is "PASS" if |check_title| is set.
  //
  // If |disposition| is CURRENT_TAB, the blocked content will be opened as
  // it was specified by renderer. In this case possible WhatToExpect is
  // kExpectPopup or kExpectForegroundTab.
  //
  // But if |disposition| is something else but CURRENT_TAB, blocked contents
  // will be opened in that alternative disposition. This is for allowing users
  // to use keyboard modifiers in ContentSettingBubble. Therefore possible
  // WhatToExpect is kExpectForegroundTab, kExpectBackgroundTab, or
  // kExpectNewWindow.
  //
  // Returns the WebContents of the launched popup.
  WebContents* RunCheckTest(Browser* browser,
                            const std::string& test_name,
                            WindowOpenDisposition disposition,
                            WhatToExpect what_to_expect,
                            ShouldCheckTitle check_title,
                            const std::u16string& expected_title = u"PASS") {
    GURL url(embedded_test_server()->GetURL(test_name));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

    // Since the popup blocker blocked the window.open, there should be only one
    // tab and window in the profile.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    ui_test_utils::TabAddedWaiter tab_add(browser);

    // Launch the blocked popup.
    blocked_content::PopupBlockerTabHelper* popup_blocker_helper =
        blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents);
    ui_test_utils::WaitForViewVisibility(browser, VIEW_ID_CONTENT_SETTING_POPUP,
                                         true);
    EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
    std::map<int32_t, GURL> blocked_requests =
        popup_blocker_helper->GetBlockedPopupRequests();
    std::map<int32_t, GURL>::const_iterator iter = blocked_requests.begin();
    ui_test_utils::BrowserChangeObserver popup_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    popup_blocker_helper->ShowBlockedPopup(iter->first, disposition);

    Browser* new_browser;
    if (what_to_expect == kExpectPopup || what_to_expect == kExpectNewWindow) {
      ui_test_utils::WaitForBrowserSetLastActive(popup_observer.Wait());
      new_browser = BrowserList::GetInstance()->GetLastActive();
      EXPECT_NE(browser, new_browser);
      web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
      if (what_to_expect == kExpectNewWindow)
        EXPECT_TRUE(new_browser->is_type_normal());
    } else {
      tab_add.Wait();
      new_browser = browser;
      EXPECT_EQ(2, browser->tab_strip_model()->count());
      int expected_active_tab =
          (what_to_expect == kExpectForegroundTab) ? 1 : 0;
      EXPECT_EQ(expected_active_tab,
                browser->tab_strip_model()->active_index());
      web_contents = browser->tab_strip_model()->GetWebContentsAt(1);
    }

    if (check_title == kCheckTitle) {
      // Check that the check passed.
      content::TitleWatcher title_watcher(web_contents, expected_title);
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }

    return web_contents;
  }
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, BlockWebContentsCreation) {
  RunCheckTest(browser(), "/popup_blocker/popup-blocked-to-post-blank.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

// TODO(crbug.com/40144522): Flaky on Mac ASAN and Chrome OS.
#if (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER)) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_BlockWebContentsCreationIncognito \
  DISABLED_BlockWebContentsCreationIncognito
#else
#define MAYBE_BlockWebContentsCreationIncognito \
  BlockWebContentsCreationIncognito
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       MAYBE_BlockWebContentsCreationIncognito) {
  RunCheckTest(CreateIncognitoBrowser(),
               "/popup_blocker/popup-blocked-to-post-blank.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupBlockedFakeClickOnAnchor) {
  RunCheckTest(browser(), "/popup_blocker/popup-fake-click-on-anchor.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchorNoTarget) {
  RunCheckTest(browser(), "/popup_blocker/popup-fake-click-on-anchor2.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupMetrics) {
  const char kPopupActions[] = "ContentSettings.Popups.BlockerActions";
  base::HistogramTester tester;

  const GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(2, GetBlockedContentsCount());

  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(
          blocked_content::PopupBlockerTabHelper::Action::kInitiated),
      2);
  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(
          blocked_content::PopupBlockerTabHelper::Action::kBlocked),
      2);

  // Click through one of them.
  auto* popup_blocker = blocked_content::PopupBlockerTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  popup_blocker->ShowBlockedPopup(
      popup_blocker->GetBlockedPopupRequests().begin()->first,
      WindowOpenDisposition::NEW_BACKGROUND_TAB);

  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(blocked_content::PopupBlockerTabHelper::Action::
                           kClickedThroughNoGesture),
      1);

  // Allowlist the site and navigate again.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(
          blocked_content::PopupBlockerTabHelper::Action::kInitiated),
      4);
  // 4 initiated popups, 2 blocked, and 1 clicked through.
  tester.ExpectTotalCount(kPopupActions, 4 + 2 + 1);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopups) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(2, GetBlockedContentsCount());
}

// Verify that popups are launched on browser back button.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSetting) {
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, kExpectForegroundTab);
}

// Verify that content settings are applied based on the top-level frame URL.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSettingIFrame) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-frames.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  // Popup from the iframe should be allowed since the top-level URL is
  // allowlisted.
  NavigateAndCheckPopupShown(url, kExpectForegroundTab);

  // Allowlist iframe URL instead.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.a.com");
  GURL frame_url(embedded_test_server()
                     ->GetURL("/popup_blocker/popup-frames-iframe.html")
                     .ReplaceComponents(replace_host));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::POPUPS);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(frame_url, GURL(),
                                      ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  // Popup should be blocked.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(1, GetBlockedContentsCount());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, NoPopupsLaunchWhenTabIsClosed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL url2(
      embedded_test_server()->GetURL("/popup_blocker/popup-success.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Expect no popup.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupsAllowedWhenPopupBlockingIsDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);

  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html"));

  NavigateAndCheckPopupShown(url, kExpectPopup);
}

// Verify that when you unblock popup, the popup shows in history and omnibox.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       UnblockedPopupShowsInHistoryAndOmnibox) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  NavigateAndCheckPopupShown(url, kExpectForegroundTab);

  // Make sure the navigation in the new tab actually finished.
  WebContents* web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  std::u16string expected_title(u"Popup Success!");
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  WaitForHistoryBackendToRun(browser()->profile());

  std::string search_string =
      "data:text/html,<title>Popup Success!</title>you should not see this "
      "message if popup blocker is enabled";

  ui_test_utils::HistoryEnumerator history(browser()->profile());
  std::vector<GURL>& history_urls = history.urls();
  ASSERT_EQ(2u, history_urls.size());
  ASSERT_EQ(embedded_test_server()->GetURL("/popup_blocker/popup-success.html"),
            history_urls[0]);
  ASSERT_EQ(url, history_urls[1]);

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  ui_test_utils::SendToOmniboxAndSubmit(browser(), search_string);
  OmniboxEditModel* model =
      browser()->window()->GetLocationBar()->GetOmniboxView()->model();
  EXPECT_EQ(GURL(search_string), model->CurrentMatch(nullptr).destination_url);
  EXPECT_EQ(base::ASCIIToUTF16(search_string),
            model->CurrentMatch(nullptr).contents);
}

// This test fails on linux AURA with this change
// https://codereview.chromium.org/23903056
// BUG=https://code.google.com/p/chromium/issues/detail?id=295299
// TODO(ananta). Debug and fix this test.
#if defined(USE_AURA) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN))
#define MAYBE_WindowFeatures DISABLED_WindowFeatures
#else
#define MAYBE_WindowFeatures WindowFeatures
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MAYBE_WindowFeatures) {
  WebContents* popup = RunCheckTest(
      browser(), "/popup_blocker/popup-window-open.html",
      WindowOpenDisposition::CURRENT_TAB, kExpectPopup, kDontCheckTitle);

  // Check that the new popup has (roughly) the requested size.
  gfx::Size window_size = popup->GetContainerBounds().size();
  EXPECT_TRUE(349 <= window_size.width() && window_size.width() <= 351);
  EXPECT_GE(window_size.height(), 249);
  EXPECT_LE(window_size.height(), 253);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CorrectReferrer) {
  RunCheckTest(browser(), "/popup_blocker/popup-referrer.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CorrectFrameName) {
  RunCheckTest(browser(), "/popup_blocker/popup-framename.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WindowFeaturesBarProps) {
  RunCheckTest(browser(), "/popup_blocker/popup-windowfeatures.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectPopup, kCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, SessionStorage) {
  RunCheckTest(browser(), "/popup_blocker/popup-sessionstorage.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Opener) {
  RunCheckTest(browser(), "/popup_blocker/popup-opener.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kCheckTitle);
}

// Tests that the popup can still close itself after navigating. This tests that
// the openedByDOM bit is preserved across blocked popups.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ClosableAfterNavigation) {
  // Open a popup.
  WebContents* popup = RunCheckTest(
      browser(), "/popup_blocker/popup-opener.html",
      WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab, kCheckTitle);

  // Navigate it elsewhere.
  content::TestNavigationObserver nav_observer(popup);
  popup->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"location.href = '/empty.html'", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  nav_observer.Wait();

  // Have it close itself.
  CloseObserver close_observer(popup);
  popup->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"window.close()", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  close_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenerSuppressed) {
  RunCheckTest(browser(), "/popup_blocker/popup-openersuppressed.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ShiftClick) {
  RunCheckTest(browser(), "/popup_blocker/popup-fake-click-on-anchor3.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectPopup, kCheckTitle,
               u"Popup Success!");
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WebUI) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-webui.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // A popup to a webui url should be blocked without ever creating a new tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, GetBlockedContentsCount());
}

// Verify that the renderer can't DOS the browser by creating arbitrarily many
// popups.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DenialOfService) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-dos.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(25, GetBlockedContentsCount());
}

// Verify that an onunload popup does not show up for about:blank.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Regress427477) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  tab->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // The popup from the unload event handler should not show up for about:blank.
  ASSERT_EQ(0, GetBlockedContentsCount());
}

// Verify that JavaScript dialogs can't be used to create popunders.
//
// Note that this is a test of the PopunderPreventer, used to ensure that
// dialogs that activate don't allow popunders. The problem is that there are no
// page-triggerable dialogs that activate any more.
//
// The dialogs that are page-triggerable don't activate, and the dialogs that
// activate aren't page-triggerable. However, in order to test the
// PopunderPreventer and ensure that it doesn't regress in functionality, in
// this test we will generate an old-style activating dialog, one that cannot
// actually be triggered by the page, to ensure that if somehow a page manages
// to do it (a bug), the popunder is still prevented.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ModalPopUnder) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, kExpectPopup);

  Browser* popup_browser = chrome::FindLastActive();
  ASSERT_NE(popup_browser, browser());

// Showing an alert will raise the tab over the popup.
#if !BUILDFLAG(IS_MAC)
  // Mac doesn't activate the browser during modal dialogs, see
  // https://crbug.com/687732 for details.
  ui_test_utils::BrowserActivationWaiter alert_waiter(browser());
#endif
  bool ignored;
  javascript_dialogs::AppModalDialogManager::GetInstance()->RunJavaScriptDialog(
      tab, tab->GetPrimaryMainFrame(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      std::u16string(), std::u16string(), base::DoNothing(), &ignored);
  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(dialog);
#if !BUILDFLAG(IS_MAC)
  if (chrome::FindLastActive() != browser())
    alert_waiter.WaitForActivation();
#endif

// Verify that after the dialog is closed, the popup is in front again.
#if !BUILDFLAG(IS_MAC)
  ui_test_utils::BrowserActivationWaiter waiter(popup_browser);
#endif
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->HandleJavaScriptDialog(tab, true, nullptr);
#if !BUILDFLAG(IS_MAC)
  waiter.WaitForActivation();
#endif
  ASSERT_EQ(popup_browser, chrome::FindLastActive());
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Tests that the print preview dialog can't be used to create popunders. The
// test was added due to a bug in MacViews that causes dialogs to activate
// their parents (https://crbug.com/1073587).
// TODO(weili): investigate why this failed on Linux and ChromeOS bots,
// and why it was flaky on Windows. https://crbug.com/1241815.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PrintPreviewPopUnder PrintPreviewPopUnder
#else
#define MAYBE_PrintPreviewPopUnder DISABLED_PrintPreviewPopUnder
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MAYBE_PrintPreviewPopUnder) {
  WebContents* original_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, kExpectPopup);

  Browser* popup_browser = chrome::FindLastActive();
  ASSERT_NE(popup_browser, browser());

  // Show a print preview dialog and confirm it doesn't activate the
  // browser window containing the original WebContents.
  content::TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  WebContents* print_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialogForTesting(original_tab);
  observer.Wait();
  observer.StopWatchingNewWebContents();
  EXPECT_EQ(popup_browser, chrome::FindLastActive());

  // Navigate away; this will close the print preview dialog.
  content::WebContentsDestroyedWatcher watcher(print_preview_dialog);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  watcher.Wait();

  // The popup is still in front and being activated.
  EXPECT_EQ(popup_browser, chrome::FindLastActive());
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

class PopupBlockerBrowserTestWithWebApps : public PopupBlockerBrowserTest {
 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Reentrancy regression test for PopunderPreventer attempting to activate a
// fullscreen web app window that is being closed; see crbug.com/331095620.
// TODO(crbug.com/335493696): Mac shims don't work with faked fullscreen.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CloseFullscreenStandaloneWebApp \
  DISABLED_CloseFullscreenStandaloneWebApp
#else
#define MAYBE_CloseFullscreenStandaloneWebApp CloseFullscreenStandaloneWebApp
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTestWithWebApps,
                       MAYBE_CloseFullscreenStandaloneWebApp) {
  GURL url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId id = web_app::InstallWebAppFromPage(browser(), url);
  Browser* app = web_app::LaunchWebAppBrowserAndWait(browser()->profile(), id);
  WebContents* tab = app->tab_strip_model()->GetActiveWebContents();
  tab->GetDelegate()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  ui_test_utils::FullscreenWaiter(app, {.tab_fullscreen = true}).Wait();

  app->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app);
}

// Tests that Ctrl+Enter/Cmd+Enter keys on a link open the background tab.
// TODO(crbug.com/40901768): Re-enable this test
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DISABLED_CtrlEnterKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::TabAddedWaiter tab_add(browser());

  bool command = false;
#if BUILDFLAG(IS_MAC)
  command = true;
#endif

  SimulateKeyPress(tab, ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   !command, false, false, command);

  tab_add.Wait();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  // Check that we create the background tab.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Tests that the tapping gesture with cntl/cmd key on a link open the
// background tab.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TapGestureWithCtrlKey DISABLED_TapGestureWithCtrlKey
#else
#define MAYBE_TapGestureWithCtrlKey TapGestureWithCtrlKey
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MAYBE_TapGestureWithCtrlKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor2.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::TabAddedWaiter tab_add(browser());

#if BUILDFLAG(IS_MAC)
  unsigned modifiers = blink::WebInputEvent::kMetaKey;
#else
  unsigned modifiers = blink::WebInputEvent::kControlKey;
#endif
  content::SimulateTapWithModifiersAt(tab, modifiers, gfx::Point(350, 250));

  tab_add.Wait();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  // Check that we create the background tab.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopupsViaPostMessage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/popup_blocker/post-message-popup.html")));
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, content::EvalJs(opener, "openPopupsAndReport();"));
}

// Test that popup blocker can show blocked contents in new foreground tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenInNewForegroundTab) {
  RunCheckTest(browser(), "/popup_blocker/popup-window-open.html",
               WindowOpenDisposition::NEW_FOREGROUND_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

// Test that popup blocker can show blocked contents in new background tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenInNewBackgroundTab) {
  RunCheckTest(browser(), "/popup_blocker/popup-window-open.html",
               WindowOpenDisposition::NEW_BACKGROUND_TAB, kExpectBackgroundTab,
               kDontCheckTitle);
}

// Test that popup blocker can show blocked contents in new window.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenInNewWindow) {
  RunCheckTest(browser(), "/popup_blocker/popup-window-open.html",
               WindowOpenDisposition::NEW_WINDOW, kExpectNewWindow,
               kDontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupsDisableBackForwardCache) {
  content::BackForwardCacheDisabledTester tester;

  // Navigate to a page with a popup that will be blocked.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/popup_blocker/popup-many.html")));
  content::RenderFrameHostWrapper rfh(browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame());
  int process_id = rfh->GetProcess()->GetID();
  int frame_routing_id = rfh->GetRoutingID();

  // Navigate to another page on the same domain. This will trigger a check on
  // whether or not the RenderFrameHost can be cached.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Ensure the RFH can not be cached due to the blocked popup.
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      process_id, frame_routing_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPopupBlockerTabHelper)));

  // Navigate to a different domain which will create a new RFH.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // Because the original RFH is not cacheable it will be deleted.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
}

// Make sure the poput is attributed to the right WebContents when it is
// triggered from a different WebContents. Regression test for
// https://crbug.com/1128495
// Flaky on windows and mac: b/40896665.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_PopupTriggeredFromDifferentWebContents \
  DISABLED_PopupTriggeredFromDifferentWebContents
#else
#define MAYBE_PopupTriggeredFromDifferentWebContents \
  PopupTriggeredFromDifferentWebContents
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       MAYBE_PopupTriggeredFromDifferentWebContents) {
  const GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-in-href.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  content::WebContents* tab_1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::TabAddedWaiter tab_Added_waiter(browser());
  SimulateMouseClickOrTapElementWithId(tab_1, "link");

  tab_Added_waiter.Wait();
  content::WebContents* tab_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(tab_1, tab_2);

  // We need to make sure the js in the new tab that comes from the href runs
  // before we perform the checks further down. Since we have no control over
  // that script we just run some more (that we do control) and wait for it to
  // finish.
  EXPECT_TRUE(
      content::ExecJs(tab_2, "", content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   tab_1->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  tab_2->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DocumentPictureInPictureIsNotConsideredForBlocking) {
  EXPECT_FALSE(blocked_content::ConsiderForPopupBlocking(
      WindowOpenDisposition::NEW_PICTURE_IN_PICTURE));
}

class PopupBlockerFencedFrameTest : public PopupBlockerBrowserTest {
 public:
  PopupBlockerFencedFrameTest() = default;
  ~PopupBlockerFencedFrameTest() override = default;

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PopupBlockerFencedFrameTest,
                       AllowPopupThroughContentSettingFencedFrame) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // The content setting of the main frame URL is set to allow popup.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  content_settings->SetContentSettingDefaultScope(main_frame_url, GURL(),
                                                  ContentSettingsType::POPUPS,
                                                  CONTENT_SETTING_ALLOW);

  // The content setting of the fenced frame URL is set to block popup.
  const GURL fenced_frame_url(embedded_test_server()->GetURL(
      "b.com", "/popup_blocker/popup-window-open.html"));
  content_settings->SetContentSettingDefaultScope(fenced_frame_url, GURL(),
                                                  ContentSettingsType::POPUPS,
                                                  CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Create a fenced frame opening a popup.
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_helper_.CreateFencedFrame(primary_main_frame_host(),
                                             fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);

  // The popup should be shown even the iframe URL is blocked, since the
  // top-level URL allows popups.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(0, GetBlockedContentsCount());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerFencedFrameTest,
                       BlockPopupThroughContentSettingFencedFrame) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // The content setting of the main frame URL is set to block popup.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  content_settings->SetContentSettingDefaultScope(main_frame_url, GURL(),
                                                  ContentSettingsType::POPUPS,
                                                  CONTENT_SETTING_BLOCK);

  // The content setting of the fenced frame URL is set to allow popup.
  const GURL fenced_frame_url(embedded_test_server()->GetURL(
      "b.com", "/popup_blocker/popup-window-open.html"));
  content_settings->SetContentSettingDefaultScope(fenced_frame_url, GURL(),
                                                  ContentSettingsType::POPUPS,
                                                  CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Create a fenced frame opening a popup.
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_helper_.CreateFencedFrame(primary_main_frame_host(),
                                             fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);

  // Popup should be blocked even the iframe URL is in AllowList, since the
  // top-level URL blocks popups.
  ASSERT_EQ(1, GetBlockedContentsCount());
}

}  // namespace
