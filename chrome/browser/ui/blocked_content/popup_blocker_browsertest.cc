// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/blocked_content/list_item_position.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using testing::_;
using testing::Return;

namespace {

// Counts the number of RenderViewHosts created.
class CountRenderViewHosts : public content::NotificationObserver {
 public:
  CountRenderViewHosts() : count_(0) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }
  ~CountRenderViewHosts() override {}

  int GetRenderViewHostCreatedCount() const { return count_; }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    count_++;
  }

  content::NotificationRegistrar registrar_;

  int count_;

  DISALLOW_COPY_AND_ASSIGN(CountRenderViewHosts);
};

class CloseObserver : public content::WebContentsObserver {
 public:
  explicit CloseObserver(WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void Wait() { close_loop_.Run(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { close_loop_.Quit(); }

 private:
  base::RunLoop close_loop_;

  DISALLOW_COPY_AND_ASSIGN(CloseObserver);
};

class PopupBlockerBrowserTest : public InProcessBrowserTest {
 public:
  PopupBlockerBrowserTest() {}
  ~PopupBlockerBrowserTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  int GetBlockedContentsCount() {
    // Do a round trip to the renderer first to flush any in-flight IPCs to
    // create a to-be-blocked window.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    if (!content::ExecuteScriptWithoutUserGesture(tab, std::string())) {
      ADD_FAILURE() << "Failed to execute script in active tab.";
      return -1;
    }
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(tab);
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
    ui_test_utils::NavigateToURL(browser(), url);

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
  WebContents* RunCheckTest(
      Browser* browser,
      const std::string& test_name,
      WindowOpenDisposition disposition,
      WhatToExpect what_to_expect,
      ShouldCheckTitle check_title,
      const base::string16& expected_title = base::ASCIIToUTF16("PASS")) {
    GURL url(embedded_test_server()->GetURL(test_name));

    CountRenderViewHosts counter;

    ui_test_utils::NavigateToURL(browser, url);

    // Since the popup blocker blocked the window.open, there should be only one
    // tab.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    // And no new RVH created.
    EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

    ui_test_utils::TabAddedWaiter tab_add(browser);

    // Launch the blocked popup.
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(web_contents);
    ui_test_utils::WaitForViewVisibility(browser, VIEW_ID_CONTENT_SETTING_POPUP,
                                         true);
    EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
    std::map<int32_t, GURL> blocked_requests =
        popup_blocker_helper->GetBlockedPopupRequests();
    std::map<int32_t, GURL>::const_iterator iter = blocked_requests.begin();
    popup_blocker_helper->ShowBlockedPopup(iter->first, disposition);

    Browser* new_browser;
    if (what_to_expect == kExpectPopup || what_to_expect == kExpectNewWindow) {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, BlockWebContentsCreation) {
  RunCheckTest(browser(), "/popup_blocker/popup-blocked-to-post-blank.html",
               WindowOpenDisposition::CURRENT_TAB, kExpectForegroundTab,
               kDontCheckTitle);
}

#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
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

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupPositionMetrics) {
  const GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(2, GetBlockedContentsCount());

  // Open two more popups.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(web_contents, "test()"));
  EXPECT_EQ(4, GetBlockedContentsCount());

  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(web_contents);
  std::vector<int32_t> ids;
  for (const auto& it : popup_blocker->GetBlockedPopupRequests())
    ids.push_back(it.first);
  ASSERT_EQ(4u, ids.size());

  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;

  base::HistogramTester tester;
  const char kClickThroughPosition[] =
      "ContentSettings.Popups.ClickThroughPosition";

  popup_blocker->ShowBlockedPopup(ids[1], disposition);
  tester.ExpectBucketCount(kClickThroughPosition,
                           static_cast<int>(ListItemPosition::kMiddleItem), 1);

  popup_blocker->ShowBlockedPopup(ids[0], disposition);
  tester.ExpectBucketCount(kClickThroughPosition,
                           static_cast<int>(ListItemPosition::kFirstItem), 1);

  popup_blocker->ShowBlockedPopup(ids[3], disposition);
  tester.ExpectBucketCount(kClickThroughPosition,
                           static_cast<int>(ListItemPosition::kLastItem), 1);

  popup_blocker->ShowBlockedPopup(ids[2], disposition);
  tester.ExpectBucketCount(kClickThroughPosition,
                           static_cast<int>(ListItemPosition::kOnlyItem), 1);

  tester.ExpectTotalCount(kClickThroughPosition, 4);

  // Requests to show popups not on the list should do nothing.
  EXPECT_FALSE(base::Contains(ids, 5));
  popup_blocker->ShowBlockedPopup(5, disposition);
  tester.ExpectTotalCount(kClickThroughPosition, 4);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupMetrics) {
  const char kPopupActions[] = "ContentSettings.Popups.BlockerActions";
  base::HistogramTester tester;

  const GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(2, GetBlockedContentsCount());

  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(PopupBlockerTabHelper::Action::kInitiated), 2);
  tester.ExpectBucketCount(
      kPopupActions, static_cast<int>(PopupBlockerTabHelper::Action::kBlocked),
      2);

  // Click through one of them.
  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  popup_blocker->ShowBlockedPopup(
      popup_blocker->GetBlockedPopupRequests().begin()->first,
      WindowOpenDisposition::NEW_BACKGROUND_TAB);

  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(PopupBlockerTabHelper::Action::kClickedThroughNoGesture),
      1);

  // Whitelist the site and navigate again.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      std::string(), CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(PopupBlockerTabHelper::Action::kInitiated), 4);
  // 4 initiated popups, 2 blocked, and 1 clicked through.
  tester.ExpectTotalCount(kPopupActions, 4 + 2 + 1);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopups) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(2, GetBlockedContentsCount());
}

// Verify that popups are launched on browser back button.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSetting) {
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      std::string(), CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, kExpectForegroundTab);
}

// Verify that content settings are applied based on the top-level frame URL.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSettingIFrame) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-frames.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                      std::string(), CONTENT_SETTING_ALLOW);

  // Popup from the iframe should be allowed since the top-level URL is
  // whitelisted.
  NavigateAndCheckPopupShown(url, kExpectForegroundTab);

  // Whitelist iframe URL instead.
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
                                      std::string(), CONTENT_SETTING_ALLOW);

  // Popup should be blocked.
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(1, GetBlockedContentsCount());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, NoPopupsLaunchWhenTabIsClosed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  GURL url2(embedded_test_server()->GetURL("/popup_blocker/"));
  ui_test_utils::NavigateToURL(browser(), url2);

  // Expect no popup.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

// This only exists for the AllowPopupsWhenTabIsClosedWithSpecialPolicy test.
// Remove this in Chrome 82. https://crbug.com/937569
class PopupBlockerSpecialPolicyBrowserTest : public PopupBlockerBrowserTest {
 public:
  PopupBlockerSpecialPolicyBrowserTest() {}
  ~PopupBlockerSpecialPolicyBrowserTest() override {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));

    policy::PolicyMap policy_map;

    policy_map.Set(policy::key::kAllowPopupsDuringPageUnload,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(true), nullptr);
    policy_provider_.UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
#else
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);
#endif
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockerSpecialPolicyBrowserTest);
};

// Remove this in Chrome 82. https://crbug.com/937569
IN_PROC_BROWSER_TEST_F(PopupBlockerSpecialPolicyBrowserTest,
                       AllowPopupsWhenTabIsClosedWithSpecialPolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  NavigateAndCheckPopupShown(embedded_test_server()->GetURL("/popup_blocker/"),
                             kExpectPopup);
}

// Verify that when you unblock popup, the popup shows in history and omnibox.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       UnblockedPopupShowsInHistoryAndOmnibox) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  NavigateAndCheckPopupShown(url, kExpectForegroundTab);

  // Make sure the navigation in the new tab actually finished.
  WebContents* web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  base::string16 expected_title(base::ASCIIToUTF16("Popup Success!"));
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
#if defined(USE_AURA) && defined(OS_LINUX)
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
#if !defined(OS_MACOSX)
  // Window height computation is off in MacViews: https://crbug.com/846329
  EXPECT_GE(window_size.height(), 249);
  EXPECT_LE(window_size.height(), 253);
#endif
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
  popup->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("location.href = '/empty.html'"), base::NullCallback());
  nav_observer.Wait();

  // Have it close itself.
  CloseObserver close_observer(popup);
  popup->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("window.close()"), base::NullCallback());
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
               base::ASCIIToUTF16("Popup Success!"));
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WebUI) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-webui.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  // A popup to a webui url should be blocked without ever creating a new tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, GetBlockedContentsCount());
}

// Verify that the renderer can't DOS the browser by creating arbitrarily many
// popups.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DenialOfService) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-dos.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(25, GetBlockedContentsCount());
}

// Verify that an onunload popup does not show up for about:blank.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Regress427477) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  tab->GetController().GoBack();
  content::WaitForLoadStop(tab);

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
                                      std::string(), CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, kExpectPopup);

  Browser* popup_browser = chrome::FindLastActive();
  ASSERT_NE(popup_browser, browser());

// Showing an alert will raise the tab over the popup.
#if !defined(OS_MACOSX)
  // Mac doesn't activate the browser during modal dialogs, see
  // https://crbug.com/687732 for details.
  ui_test_utils::BrowserActivationWaiter alert_waiter(browser());
#endif
  bool ignored;
  app_modal::JavaScriptDialogManager::GetInstance()->RunJavaScriptDialog(
      tab, tab->GetMainFrame(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      base::string16(), base::string16(), base::DoNothing(), &ignored);
  app_modal::JavaScriptAppModalDialog* dialog =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(dialog);
#if !defined(OS_MACOSX)
  if (chrome::FindLastActive() != browser())
    alert_waiter.WaitForActivation();
#endif

// Verify that after the dialog is closed, the popup is in front again.
#if !defined(OS_MACOSX)
  ui_test_utils::BrowserActivationWaiter waiter(popup_browser);
#endif
  app_modal::JavaScriptDialogManager::GetInstance()->HandleJavaScriptDialog(
      tab, true, nullptr);
#if !defined(OS_MACOSX)
  waiter.WaitForActivation();
#endif
  ASSERT_EQ(popup_browser, chrome::FindLastActive());
}

// Tests that Ctrl+Enter/Cmd+Enter keys on a link open the backgournd tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CtrlEnterKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::TabAddedWaiter tab_add(browser());

  bool command = false;
#if defined(OS_MACOSX)
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
// backgournd tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, TapGestureWithCtrlKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor2.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::TabAddedWaiter tab_add(browser());

#if defined(OS_MACOSX)
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
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/popup_blocker/post-message-popup.html"));
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  int popups = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      opener, "openPopupsAndReport();", &popups));
  EXPECT_EQ(1, popups);
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

  const GURL url1(
      embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  content::RenderFrameHost* main_frame =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  int process_id = main_frame->GetProcess()->GetID();
  int frame_routing_id = main_frame->GetRoutingID();
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));

  // Navigate to the page
  ui_test_utils::NavigateToURL(browser(), url1);
  // Navigate away while having blocked popups. This should block bfcache.
  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(process_id, frame_routing_id,
                                                  "PopupBlockerTabHelper"));
}

}  // namespace
