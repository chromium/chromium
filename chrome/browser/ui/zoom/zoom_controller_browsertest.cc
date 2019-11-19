// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/zoom_controller.h"

#include "base/macros.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using zoom::ZoomChangedWatcher;
using zoom::ZoomController;
using zoom::ZoomObserver;

class ZoomControllerBrowserTest : public InProcessBrowserTest {
 public:
  ZoomControllerBrowserTest() {}
  ~ZoomControllerBrowserTest() override {}

  void TestResetOnNavigation(ZoomController::ZoomMode zoom_mode) {
    DCHECK(zoom_mode == ZoomController::ZOOM_MODE_ISOLATED ||
           zoom_mode == ZoomController::ZOOM_MODE_MANUAL);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), GURL("about:blank"), 1);
    ZoomController* zoom_controller =
        ZoomController::FromWebContents(web_contents);
    double zoom_level = zoom_controller->GetDefaultZoomLevel();
    zoom_controller->SetZoomMode(zoom_mode);

    // When the navigation occurs, the zoom_mode will be reset to
    // ZOOM_MODE_DEFAULT, and this will be reflected in the event that
    // is generated.
    ZoomController::ZoomChangedEventData zoom_change_data(
        web_contents, zoom_level, zoom_level, ZoomController::ZOOM_MODE_DEFAULT,
        false);
    ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);

    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));
    zoom_change_watcher.Wait();
  }
};  // ZoomControllerBrowserTest

#if defined(OS_ANDROID)
#define MAYBE_CrashedTabsDoNotChangeZoom DISABLED_CrashedTabsDoNotChangeZoom
#else
#define MAYBE_CrashedTabsDoNotChangeZoom CrashedTabsDoNotChangeZoom
#endif
IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest,
                       MAYBE_CrashedTabsDoNotChangeZoom) {
  // At the start of the test we are at a tab displaying about:blank.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  content::RenderProcessHost* host = web_contents->GetMainFrame()->GetProcess();
  {
    content::RenderProcessHostWatcher crash_observer(
        host, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    host->Shutdown(0);
    crash_observer.Wait();
  }
  EXPECT_FALSE(web_contents->GetRenderViewHost()->IsRenderViewLive());

  // The following attempt to change the zoom level for a crashed tab should
  // fail.
  zoom_controller->SetZoomLevel(new_zoom_level);
  EXPECT_FLOAT_EQ(old_zoom_level, zoom_controller->GetZoomLevel());
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, OnPreferenceChanged) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  double new_default_zoom_level = 1.0;
  // Since this page uses the default zoom level, the changes to the default
  // zoom level will change the zoom level for this web_contents.
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      new_default_zoom_level,
      new_default_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      false);
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);
  // TODO(wjmaclean): Convert this to call partition-specific zoom level prefs
  // when they become available.
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
      new_default_zoom_level);
  // Because this test relies on a round-trip IPC to/from the renderer process,
  // we need to wait for it to propagate.
  zoom_change_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, ErrorPagesCanZoom) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://kjfhkjsdf.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  EXPECT_EQ(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  // The following attempt to change the zoom level for an error page should
  // fail.
  zoom_controller->SetZoomLevel(new_zoom_level);
  EXPECT_FLOAT_EQ(new_zoom_level, zoom_controller->GetZoomLevel());
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest,
                       ErrorPagesCanZoomAfterTabRestore) {
  // This url is meant to cause a network error page to be loaded.
  // Tests can't reach the network, so this test should continue
  // to work even if the domain listed is someday registered.
  GURL url("http://kjfhkjsdf.com");

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  {
    content::WebContents* web_contents = tab_strip->GetActiveWebContents();

    EXPECT_EQ(
        content::PAGE_TYPE_ERROR,
        web_contents->GetController().GetLastCommittedEntry()->GetPageType());

    content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
    tab_strip->CloseWebContentsAt(tab_strip->active_index(),
                                  TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }
  EXPECT_EQ(1, tab_strip->count());

  content::WebContentsAddedObserver new_web_contents_observer;
  chrome::RestoreTab(browser());
  content::WebContents* web_contents =
      new_web_contents_observer.GetWebContents();
  content::WaitForLoadStop(web_contents);

  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  // The following attempt to change the zoom level for an error page should
  // fail.
  zoom_controller->SetZoomLevel(new_zoom_level);
  EXPECT_FLOAT_EQ(new_zoom_level, zoom_controller->GetZoomLevel());
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, Observe) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  double new_zoom_level = 1.0;
  // When the event is initiated from HostZoomMap, the old zoom level is not
  // available.
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      new_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      false);  // The ZoomController did not initiate, so this will be 'false'.
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(
          web_contents->GetBrowserContext());

  host_zoom_map->SetZoomLevelForHost("about:blank", new_zoom_level);
  zoom_change_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, ObserveDisabledModeEvent) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  double new_zoom_level = default_zoom_level + 1.0;
  zoom_controller->SetZoomLevel(new_zoom_level);

  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      new_zoom_level,
      default_zoom_level,
      ZoomController::ZOOM_MODE_DISABLED,
      true);
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);
  zoom_controller->SetZoomMode(ZoomController::ZOOM_MODE_DISABLED);
  zoom_change_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, PerTabModeResetSendsEvent) {
  TestResetOnNavigation(ZoomController::ZOOM_MODE_ISOLATED);
}

// Regression test: crbug.com/450909.
IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, NavigationResetsManualMode) {
  TestResetOnNavigation(ZoomController::ZOOM_MODE_MANUAL);
}

#if !defined(OS_CHROMEOS)
// Regression test: crbug.com/438979.
IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest,
                       SettingsZoomAfterSigninWorks) {
  GURL signin_url(std::string(chrome::kChromeUIChromeSigninURL)
                      .append("?access_point=0&reason=5"));
  // We open the signin page in a new tab so that the ZoomController is
  // created against the HostZoomMap of the special StoragePartition that
  // backs the signin page. When we subsequently navigate away from the
  // signin page, the HostZoomMap changes, and we need to test that the
  // ZoomController correctly detects this.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), signin_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  login_ui_test_utils::WaitUntilUIReady(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  EXPECT_EQ(signin_url, web_contents->GetLastCommittedURL());
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  GURL settings_url(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), settings_url);
  EXPECT_NE(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  // Verify new tab was created.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  // Verify that the settings page is using the same WebContents.
  EXPECT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());
  // TODO(wjmaclean): figure out why this next line fails, i.e. why does this
  // test not properly trigger a navigation to the settings page.
  EXPECT_EQ(settings_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(zoom_controller, ZoomController::FromWebContents(web_contents));

  // If we zoom the new page, it should still generate a ZoomController event.
  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      old_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      true);  // We have a non-empty host, so this will be 'true'.
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);
  zoom_controller->SetZoomLevel(new_zoom_level);
  zoom_change_watcher.Wait();
}
#endif  // !defined(OS_CHROMEOS)
