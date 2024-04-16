// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WindowManagementTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    display::Screen::SetScreenInstance(&screen_);
    screen_.display_list().AddDisplay({1, gfx::Rect(0, 0, 803, 600)},
                                      display::DisplayList::Type::PRIMARY);
#endif
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Window management features are only available on secure contexts.
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    display::Screen::SetScreenInstance(nullptr);
#endif
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::ScreenBase screen_;
#endif
};

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
#if BUILDFLAG(IS_WIN)
#define MAYBE_NoCrashOnEventsDuringHandlerPrint \
  DISABLED_NoCrashOnEventsDuringHandlerPrint
#else
#define MAYBE_NoCrashOnEventsDuringHandlerPrint \
  NoCrashOnEventsDuringHandlerPrint
#endif
// Test that screen change events occurring while an event handler is running
// a nested event loop (i.e. via window.print()) do not cause a crash.
// Regression test for crbug.com/1273841
IN_PROC_BROWSER_TEST_F(WindowManagementTest,
                       MAYBE_NoCrashOnEventsDuringHandlerPrint) {
  // Update the display configuration to mock display changes.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-803x600");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  // Navigate in a new tab to observe the test screen instance as needed.
  const GURL url(https_test_server_->GetURL("/simple.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Auto-accept the Window Placement permission request.
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager* permission_request_manager_tab =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager_tab->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Add a currentscreenchange event handler that will freeze JS via print().
  auto* script = R"(
      function freezeJS(e) {
        self.print();
        screenDetails.removeEventListener('currentscreenchange', freezeJS);
      }

      let screenDetails;
      self.getScreenDetails().then(s => {
        screenDetails = s;
        screenDetails.addEventListener('currentscreenchange', freezeJS);
      });
  )";
  content::WebContentsAddedObserver web_contents_added_observer;
  ASSERT_TRUE(EvalJs(tab, script).error.empty());

  // Alter the display to trigger the currentscreenchange event and print().
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-807x600");
#else
  screen_.display_list().UpdateDisplay({1, gfx::Rect(0, 0, 807, 600)},
                                       display::DisplayList::Type::PRIMARY);
  EXPECT_EQ(screen_.display_list().displays().size(), 1u);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  content::WebContents* print_preview =
      web_contents_added_observer.GetWebContents();
  EXPECT_EQ(print_preview, printing::PrintPreviewDialogController::GetInstance()
                               ->GetPrintPreviewForContents(tab));
  content::AwaitDocumentOnLoadCompleted(print_preview);

  // Add a second display while the print preview dialog is showing.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-807x600,1000+0-804x600");
#else
  screen_.display_list().AddDisplay({2, gfx::Rect(1000, 0, 804, 600)},
                                    display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(screen_.display_list().displays().size(), 2u);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  // Cancel print to unfreeze the tab's JS and check that no crash occurred.
  content::WebContentsDestroyedWatcher destroyed_watcher(print_preview);
  // Note: This could be a browser_test if cancelling the print preview dialog
  // was more broadly possible without simulating a key press.
  // PrintViewManager::PrintPreviewDone() worked on Linux, but not Chrome OS.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  destroyed_watcher.Wait();
  ASSERT_FALSE(tab->IsCrashed());
  EXPECT_EQ(true, EvalJs(tab, "true"));
}
