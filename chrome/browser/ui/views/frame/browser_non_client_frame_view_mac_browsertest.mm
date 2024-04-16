// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

namespace {

class TextChangeWaiter {
 public:
  explicit TextChangeWaiter(views::Label* label)
      : subscription_(label->AddTextChangedCallback(
            base::BindRepeating(&TextChangeWaiter::OnTextChanged,
                                base::Unretained(this)))) {}

  TextChangeWaiter(const TextChangeWaiter&) = delete;
  TextChangeWaiter& operator=(const TextChangeWaiter&) = delete;

  // Runs a loop until a text change is observed (unless one has
  // already been observed, in which case it returns immediately).
  void Wait() {
    if (observed_change_)
      return;

    run_loop_.Run();
  }

 private:
  void OnTextChanged() {
    observed_change_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool observed_change_ = false;
  base::RunLoop run_loop_;
  base::CallbackListSubscription subscription_;
};

}  // anonymous namespace

enum class PrefixTitles { kEnabled, kDisabled };

using BrowserNonClientFrameViewMacBrowserTestTitlePrefixed =
    web_app::WebAppBrowserTestBase;

// This will always be flaky on mac due to RemoteCocoa, the way it mocks out
// fullscreen doesn't play with remote cocoa. So it gets true fullscreen (which
// would probably be flaky in browser tests), but then also hits the DCHECK
// because it tests real full screen. https://crbug.com/333417404
#if BUILDFLAG(IS_MAC)
#define MAYBE_TitleUpdates DISABLED_TitleUpdates
#else
#define MAYBE_TitleUpdates TitleUpdates
#endif
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewMacBrowserTestTitlePrefixed,
                       MAYBE_TitleUpdates) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Ensure the main page has loaded and is ready for ExecJs DOM manipulation.
  ASSERT_TRUE(content::NavigateToURL(web_contents, start_url));

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::NonClientFrameView* const frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();
  auto* const title =
      static_cast<views::Label*>(frame_view->GetViewByID(VIEW_ID_WINDOW_TITLE));

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(u"A Web App - Full Screen", title->GetText());
  }

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(u"A Web App - Not Full Screen", title->GetText());
  }
}

using BrowserNonClientFrameViewMacBrowserTest = web_app::WebAppBrowserTestBase;

// Test to make sure the WebAppToolbarFrame triggers an InvalidateLayout() when
// toggled in fullscreen mode.
// TODO(crbug.com/40735737): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ToolbarLayoutFullscreenTransition \
  DISABLED_ToolbarLayoutFullscreenTransition
#else
#define MAYBE_ToolbarLayoutFullscreenTransition \
  ToolbarLayoutFullscreenTransition
#endif
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewMacBrowserTest,
                       MAYBE_ToolbarLayoutFullscreenTransition) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserNonClientFrameView* const frame_view =
      static_cast<BrowserNonClientFrameView*>(
          browser_view->GetWidget()->non_client_view()->frame_view());

  // Trigger a layout on the view tree to address any invalid layouts waiting
  // for a re-layout.
  views::ViewTestApi frame_view_test_api(frame_view);
  browser_view->GetWidget()->LayoutRootViewIfNecessary();

  // Assert that the layout of the frame view is in a valid state.
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, false);

  chrome::ToggleFullscreenMode(browser);
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);

  // Showing the toolbar in fullscreen mode should trigger a layout
  // invalidation.
  EXPECT_TRUE(frame_view_test_api.needs_layout());
}
