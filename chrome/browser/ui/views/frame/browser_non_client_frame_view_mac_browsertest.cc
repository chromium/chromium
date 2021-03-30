// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

class BrowserNonClientFrameViewMacBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  BrowserNonClientFrameViewMacBrowserTest() = default;
  BrowserNonClientFrameViewMacBrowserTest(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  BrowserNonClientFrameViewMacBrowserTest& operator=(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  ~BrowserNonClientFrameViewMacBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewMacBrowserTest, TitleUpdates) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const web_app::AppId app_id = InstallPWA(start_url);
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
    const std::u16string expected_title(u"Full Screen");
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    const std::u16string expected_title(u"Not Full Screen");
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }
}

// Test to make sure the WebAppToolbarFrame triggers an InvalidateLayout() when
// toggled in fullscreen mode.
// TODO(crbug.com/1156050): Flaky on Mac.
#if defined(OS_MAC)
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
  const web_app::AppId app_id = InstallPWA(start_url);
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
  fake_fullscreen.FinishTransition();
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);

  // Showing the toolbar in fullscreen mode should trigger a layout
  // invalidation.
  EXPECT_TRUE(frame_view_test_api.needs_layout());
}
