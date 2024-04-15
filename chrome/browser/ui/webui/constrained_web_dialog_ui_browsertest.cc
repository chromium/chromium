// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

using content::WebContents;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;

namespace {

static const char kTestDataURL[] = "data:text/html,<!doctype html>"
    "<body></body>"
    "<style>"
    "body { height: 150px; width: 150px; }"
    "</style>";

bool IsEqualSizes(gfx::Size expected,
                  ConstrainedWebDialogDelegate* dialog_delegate) {
  return expected == dialog_delegate->GetConstrainedWebDialogPreferredSize();
}

std::string GetChangeDimensionsScript(int dimension) {
  return base::StringPrintf("window.document.body.style.width = %d + 'px';"
      "window.document.body.style.height = %d + 'px';", dimension, dimension);
}

class AutoResizingTestWebDialogDelegate
    : public ui::test::TestWebDialogDelegate {
 public:
  explicit AutoResizingTestWebDialogDelegate(const GURL& url)
      : TestWebDialogDelegate(url) {}
  ~AutoResizingTestWebDialogDelegate() override {}

  // Dialog delegates for auto-resizing dialogs are expected not to set |size|.
  void GetDialogSize(gfx::Size* size) const override {}
};

}  // namespace

class ConstrainedWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedWebDialogBrowserTest() {}

  // Runs the current MessageLoop until |condition| is true or timeout.
  bool RunLoopUntil(base::RepeatingCallback<bool()> condition) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    while (!condition.Run()) {
      const base::TimeTicks current_time = base::TimeTicks::Now();
      if (current_time - start_time > base::Seconds(5)) {
        ADD_FAILURE() << "Condition not met within five seconds.";
        return false;
      }

      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(20));
      run_loop.Run();
    }
    return true;
  }

 protected:
  bool IsShowingWebContentsModalDialog(WebContents* web_contents) const {
    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(web_contents);
    return web_contents_modal_dialog_manager->IsDialogActive();
  }
};

// Tests that opening/closing the constrained window won't crash it.
// Flaky on trusty builder: http://crbug.com/1020490.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_BasicTest DISABLED_BasicTest
#else
#define MAYBE_BasicTest BasicTest
#endif
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest, MAYBE_BasicTest) {
  auto delegate = std::make_unique<ui::test::TestWebDialogDelegate>(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate = ShowConstrainedWebDialog(
      browser()->profile(), std::move(delegate), web_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(web_contents));
}

// TODO(crbug.com/40656271): Crashy on Linux
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ReleaseWebContents DISABLED_ReleaseWebContents
#else
#define MAYBE_ReleaseWebContents ReleaseWebContents
#endif
// Tests that ReleaseWebContents() works.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       MAYBE_ReleaseWebContents) {
  auto delegate = std::make_unique<ui::test::TestWebDialogDelegate>(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate = ShowConstrainedWebDialog(
      browser()->profile(), std::move(delegate), web_contents);
  ASSERT_TRUE(dialog_delegate);
  WebContents* dialog_contents = dialog_delegate->GetWebContents();
  ASSERT_TRUE(dialog_contents);
  ASSERT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  content::WebContentsDestroyedWatcher watcher(dialog_contents);
  std::unique_ptr<WebContents> dialog_contents_holder =
      dialog_delegate->ReleaseWebContents();
  dialog_delegate->OnDialogCloseFromWebUI();

  ASSERT_FALSE(watcher.IsDestroyed());
  EXPECT_FALSE(IsShowingWebContentsModalDialog(web_contents));
  dialog_contents_holder.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
}

// Tests that dialog autoresizes based on web contents when autoresizing
// is enabled.
// Flaky on CrOS: http://crbug.com/928924
// Flaky on Mac: http://crbug.com/1498848
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ContentResizeInAutoResizingDialog \
  DISABLED_ContentResizeInAutoResizingDialog
#else
#define MAYBE_ContentResizeInAutoResizingDialog \
  ContentResizeInAutoResizingDialog
#endif
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       MAYBE_ContentResizeInAutoResizingDialog) {
  // During auto-resizing, dialogs size to (WebContents size) + 16.
  const int dialog_border_space = 16;

  // Expected dialog sizes after auto-resizing.
  const int initial_size = 150 + dialog_border_space;
  const int new_size = 175 + dialog_border_space;

  auto delegate =
      std::make_unique<AutoResizingTestWebDialogDelegate>(GURL(kTestDataURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Observes the next created WebContents.
  content::TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();

  gfx::Size min_size = gfx::Size(100, 100);
  gfx::Size max_size = gfx::Size(200, 200);
  gfx::Size initial_dialog_size;

  delegate->GetDialogSize(&initial_dialog_size);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialogWithAutoResize(browser()->profile(),
                                             std::move(delegate), web_contents,
                                             min_size, max_size);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  ASSERT_FALSE(IsShowingWebContentsModalDialog(web_contents));
  EXPECT_EQ(min_size, dialog_delegate->GetConstrainedWebDialogMinimumSize());
  EXPECT_EQ(max_size, dialog_delegate->GetConstrainedWebDialogMaximumSize());

  // Check for initial sizing. Dialog was created as a 400x400 dialog.
  ASSERT_EQ(initial_dialog_size,
            dialog_delegate->GetConstrainedWebDialogPreferredSize());

  observer.Wait();

  // Wait until the entire WebContents has loaded.
  EXPECT_TRUE(WaitForLoadStop(dialog_delegate->GetWebContents()));

  ASSERT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  // Resize to content's originally set dimensions.
  ASSERT_TRUE(RunLoopUntil(base::BindRepeating(
      &IsEqualSizes, gfx::Size(initial_size, initial_size), dialog_delegate)));

  // Resize to dimensions within expected bounds.
  EXPECT_TRUE(ExecJs(dialog_delegate->GetWebContents(),
                     GetChangeDimensionsScript(175)));
  ASSERT_TRUE(RunLoopUntil(base::BindRepeating(
      &IsEqualSizes, gfx::Size(new_size, new_size), dialog_delegate)));

  // Resize to dimensions smaller than the minimum bounds.
  EXPECT_TRUE(
      ExecJs(dialog_delegate->GetWebContents(), GetChangeDimensionsScript(50)));
  ASSERT_TRUE(RunLoopUntil(
      base::BindRepeating(&IsEqualSizes, min_size, dialog_delegate)));

  // Resize to dimensions greater than the maximum bounds.
  EXPECT_TRUE(ExecJs(dialog_delegate->GetWebContents(),
                     GetChangeDimensionsScript(250)));
  ASSERT_TRUE(RunLoopUntil(
      base::BindRepeating(&IsEqualSizes, max_size, dialog_delegate)));
}

// Tests that dialog does not autoresize when autoresizing is not enabled.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ContentResizeInNonAutoResizingDialog) {
  auto delegate =
      std::make_unique<ui::test::TestWebDialogDelegate>(GURL(kTestDataURL));
  auto* delegate_ptr = delegate.get();
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate = ShowConstrainedWebDialog(
      browser()->profile(), std::move(delegate), web_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  // Wait until the entire WebContents has loaded.
  EXPECT_TRUE(WaitForLoadStop(dialog_delegate->GetWebContents()));

  gfx::Size initial_dialog_size;
  delegate_ptr->GetDialogSize(&initial_dialog_size);

  // Check for initial sizing. Dialog was created as a 400x400 dialog.
  ASSERT_EQ(initial_dialog_size,
            dialog_delegate->GetConstrainedWebDialogPreferredSize());

  // Resize <body> to dimension smaller than dialog.
  EXPECT_TRUE(ExecJs(dialog_delegate->GetWebContents(),
                     GetChangeDimensionsScript(100)));
  ASSERT_TRUE(RunLoopUntil(base::BindRepeating(
      &IsEqualSizes, initial_dialog_size, dialog_delegate)));

  // Resize <body> to dimension larger than dialog.
  EXPECT_TRUE(ExecJs(dialog_delegate->GetWebContents(),
                     GetChangeDimensionsScript(500)));
  ASSERT_TRUE(RunLoopUntil(base::BindRepeating(
      &IsEqualSizes, initial_dialog_size, dialog_delegate)));
}
