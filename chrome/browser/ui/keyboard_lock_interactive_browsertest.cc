// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/fullscreen_keyboard_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace {

// Javascript snippet used to verify the keyboard lock API exists.
// TODO(crbug.com/40501396): These checks can be removed once the blink flag for
// the API is removed.
constexpr char kKeyboardLockMethodExistanceCheck[] =
    "(navigator.keyboard != undefined) &&"
    "(navigator.keyboard.lock != undefined);";

// Javascript snippet used to request that all keys be locked.
constexpr char kKeyboardLockMethodCallWithAllKeys[] =
    "navigator.keyboard.lock().then("
    "  () => true,"
    "  () => false,"
    ");";

// Javascript snippet used to request that the 'T' key be locked.  This means
// The Ctrl+T browser shortcut will be intercepted, but other shortcuts should
// continue to function.
constexpr char kKeyboardLockMethodCallWithSomeKeys[] =
    "navigator.keyboard.lock(['KeyT']).then("
    "  () => true,"
    "  () => false,"
    ");";

// Javascript snippet used to request that the 'escape' key be locked.  This
// means that all browser shortcuts will continue to work however the user would
// need to press and hold escape to exit tab-initiated fullscreen.
constexpr char kKeyboardLockMethodCallWithEscapeKey[] =
    "navigator.keyboard.lock(['Escape']).then("
    "  () => true,"
    "  () => false,"
    ");";

// Javascript snippet used to release all locked keys.
constexpr char kKeyboardUnlockMethodCall[] = "navigator.keyboard.unlock()";

// Path to a simple html fragment, used for navigation tests.
constexpr char kSimplePageHTML[] = "/title1.html";

// The test data folder path used for download tests.
constexpr char kDownloadFolder[] = "downloads";

// Name of the test file used for download tests.
constexpr char kDownloadFile[] = "a_zip_file.zip";

}  // namespace

// Test fixture which sets up the environment and provides helper methods for
// testing keyboard lock functionality at the browser UI level.
class KeyboardLockInteractiveBrowserTest
    : public FullscreenKeyboardBrowserTestBase {
 public:
  KeyboardLockInteractiveBrowserTest();

  KeyboardLockInteractiveBrowserTest(
      const KeyboardLockInteractiveBrowserTest&) = delete;
  KeyboardLockInteractiveBrowserTest& operator=(
      const KeyboardLockInteractiveBrowserTest&) = delete;

  ~KeyboardLockInteractiveBrowserTest() override;

  // FullscreenKeyboardBrowserTestBase implementation.
  net::EmbeddedTestServer* GetEmbeddedTestServer() override;

 protected:
  // InProcessBrowserTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Helper methods for common tasks.
  bool KeyboardLockApiExists();
  bool IsKeyboardLockActive();
  bool IsKeyboardLockRequestRegistered();
  bool RequestKeyboardLock(bool lock_all_keys = true);
  bool CancelKeyboardLock();
  bool DisablePreventDefaultOnTestPage();
#if BUILDFLAG(IS_MAC)
  void ExitFullscreen();
#endif

  ExclusiveAccessManager* GetExclusiveAccessManager() {
    return browser()->exclusive_access_manager();
  }

  KeyboardLockController* GetKeyboardLockController() {
    return GetExclusiveAccessManager()->keyboard_lock_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_test_server_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ui::test::ScopedFakeNSWindowFullscreen> fake_fullscreen_ =
      std::make_unique<ui::test::ScopedFakeNSWindowFullscreen>();
#endif
};

KeyboardLockInteractiveBrowserTest::KeyboardLockInteractiveBrowserTest()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

KeyboardLockInteractiveBrowserTest::~KeyboardLockInteractiveBrowserTest() =
    default;

void KeyboardLockInteractiveBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // It is important to disable system keyboard lock as the low-level test
  // utility functions install a keyboard hook to listen for key events and the
  // keyboard lock hook can interfere with it.
  // Turn off Paint Holding because the content used in the test does not paint
  // anything and we do not want to wait for the timeout.
  scoped_feature_list_.InitWithFeatures(
      {}, {features::kSystemKeyboardLock, blink::features::kPaintHolding});
}

void KeyboardLockInteractiveBrowserTest::SetUpOnMainThread() {
  GetEmbeddedTestServer()->AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(GetEmbeddedTestServer()->Start());
  FullscreenKeyboardBrowserTestBase::SetUpOnMainThread();
}

net::EmbeddedTestServer*
KeyboardLockInteractiveBrowserTest::GetEmbeddedTestServer() {
  // KeyboardLock requires a secure context (HTTPS).  The default test server
  // uses HTTP so we use our own test server which we initialize to use HTTPS.
  return &https_test_server_;
}

bool KeyboardLockInteractiveBrowserTest::KeyboardLockApiExists() {
  return EvalJs(GetActiveWebContents(), kKeyboardLockMethodExistanceCheck)
      .ExtractBool();
}

bool KeyboardLockInteractiveBrowserTest::IsKeyboardLockActive() {
  return GetActiveWebContents()->GetRenderWidgetHostView()->IsKeyboardLocked();
}

bool KeyboardLockInteractiveBrowserTest::IsKeyboardLockRequestRegistered() {
  return content::GetKeyboardLockWidget(GetActiveWebContents()) != nullptr;
}

bool KeyboardLockInteractiveBrowserTest::RequestKeyboardLock(
    bool lock_all_keys /*=true*/) {
  // keyboard.lock() is an async call which requires a promise handling dance.
  return EvalJs(GetActiveWebContents(),
                lock_all_keys ? kKeyboardLockMethodCallWithAllKeys
                              : kKeyboardLockMethodCallWithSomeKeys)
      .ExtractBool();
}

bool KeyboardLockInteractiveBrowserTest::CancelKeyboardLock() {
  // keyboard.unlock() is a synchronous call.
  return ExecJs(GetActiveWebContents(), kKeyboardUnlockMethodCall);
}

#if BUILDFLAG(IS_MAC)
void KeyboardLockInteractiveBrowserTest::ExitFullscreen() {
  fake_fullscreen_.reset();
}
#endif

bool KeyboardLockInteractiveBrowserTest::DisablePreventDefaultOnTestPage() {
  // We cannot test browser shortcuts in JS fullscreen with the default webpage
  // behavior as it will prevent default on every keypress.  Since we want to
  // test whether the browser does the right thing when receiving a shortcut, we
  // tell the test webpage not to prevent default on key events.
  // Note that some tests will want the prevent default behavior to ensure
  // certain keys, such as escape, cannot be prevented by the webpage.
  return ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_D, false,
                                         false, false, false);
}

// https://crbug.com/1382717 Flaky on Linux
#if BUILDFLAG(IS_LINUX)
#define MAYBE_RequestedButNotActive DISABLED_RequestedButNotActive
#else
#define MAYBE_RequestedButNotActive RequestedButNotActive
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_RequestedButNotActive) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());
  ASSERT_TRUE(KeyboardLockApiExists());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Requesting keyboard lock does not engage until tab-initiated fullscreen.
  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Common browser shortcuts (new tab/window) should take effect.
  ASSERT_NO_FATAL_FAILURE(VerifyShortcutsAreNotPrevented());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       ActiveWithAllKeysLocked) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());
  ASSERT_TRUE(KeyboardLockApiExists());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());
  ASSERT_FALSE(IsInBrowserFullscreen());
  ASSERT_FALSE(IsActiveTabFullscreen());

  // Requesting keyboard lock does not engage until tab-initiated fullscreen.
  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Tab-initiated fullscreen (JS API) does engage keyboard lock.
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_FALSE(IsInBrowserFullscreen());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press does not exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_FALSE(IsInBrowserFullscreen());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Common browser shortcuts (new tab/window) should not take effect.
  ASSERT_NO_FATAL_FAILURE(SendShortcutsAndExpectPrevented());
}

// https://crbug.com/1382699 Flaky on Linux
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ActiveWithSomeKeysLocked DISABLED_ActiveWithSomeKeysLocked
#else
#define MAYBE_ActiveWithSomeKeysLocked ActiveWithSomeKeysLocked
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_ActiveWithSomeKeysLocked) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());
  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));

  ASSERT_NO_FATAL_FAILURE(VerifyShortcutsAreNotPrevented());

  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  // New Tab shortcut is prevented.
  int initial_tab_count = GetTabCount();
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  ASSERT_EQ(initial_tab_count, GetTabCount());
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  ASSERT_EQ(initial_tab_count, GetTabCount());

  // New Window shortcut is not prevented.
  size_t initial_browser_count = GetBrowserCount();
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_N));
  WaitForBrowserCount(initial_browser_count + 1);
  ASSERT_EQ(initial_browser_count + 1, GetBrowserCount());
}

// https://crbug.com/1108391 Flakey on ChromeOS.
// https://crbug.com/1121172 Also flaky on Lacros and Mac
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_SubsequentLockCallSupersedesPreviousCall \
  DISABLED_SubsequentLockCallSupersedesPreviousCall
#else
#define MAYBE_SubsequentLockCallSupersedesPreviousCall \
  SubsequentLockCallSupersedesPreviousCall
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_SubsequentLockCallSupersedesPreviousCall) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  // First we lock all keys.
  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/true));
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press does not exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Shortcuts are now prevented from having an effect.
  ASSERT_NO_FATAL_FAILURE(SendShortcutsAndExpectPrevented());

  // Now, Only lock the escape key.
  ASSERT_EQ(true, EvalJs(GetActiveWebContents(),
                         kKeyboardLockMethodCallWithEscapeKey));
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press does not exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Common shortcuts should work now.
  int initial_tab_count = GetTabCount();
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_EQ(initial_tab_count + 1, GetTabCount());
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  WaitForTabCount(initial_tab_count);
  ASSERT_EQ(initial_tab_count, GetTabCount());

  // Creating a new tab will kick us out of fullscreen, verify that and then
  // request fullscreen again.
  ASSERT_FALSE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Lock all keys again.
  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/true));
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press does not exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Shortcuts are prevented from having an effect.
  ASSERT_NO_FATAL_FAILURE(SendShortcutsAndExpectPrevented());

  // Last, update the set of keys being requested so escape is not locked.
  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press will now exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_FALSE(IsActiveTabFullscreen());
  ASSERT_FALSE(IsKeyboardLockActive());
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/41385780): Enable once browser fullscreen is reliable in
// tests.
#define MAYBE_RequestedButNotActiveInBrowserFullscreen \
  DISABLED_RequestedButNotActiveInBrowserFullscreen
#else
#define MAYBE_RequestedButNotActiveInBrowserFullscreen \
  RequestedButNotActiveInBrowserFullscreen
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_RequestedButNotActiveInBrowserFullscreen) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());
  ASSERT_TRUE(KeyboardLockApiExists());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Requesting keyboard lock does not engage until tab-initiated fullscreen.
  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Browser fullscreen (F11) does not engage keyboard lock.
  ASSERT_NO_FATAL_FAILURE(SendFullscreenShortcutAndWait());
  ASSERT_TRUE(IsInBrowserFullscreen());
  ASSERT_FALSE(IsActiveTabFullscreen());
  ASSERT_FALSE(IsKeyboardLockActive());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       CancelActiveKeyboardLockInFullscreen) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  // Requesting keyboard lock does not engage until tab-initiated fullscreen.
  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Tab-initiated fullscreen (JS API) does engage keyboard lock.
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_FALSE(IsInBrowserFullscreen());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Common browser shortcuts (new tab/window) should not take effect.
  ASSERT_NO_FATAL_FAILURE(SendShortcutsAndExpectPrevented());

  // Cancel keyboard lock while in fullscreen.
  ASSERT_TRUE(CancelKeyboardLock());
  ASSERT_FALSE(IsKeyboardLockActive());

  // New Tab shortcut is no longer prevented.
  int initial_tab_count = GetTabCount();
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_EQ(initial_tab_count + 1, GetTabCount());
}

// TODO(crbug.com/40827037): Flaky on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelActiveKeyboardLockBeforeFullscreen \
  DISABLED_CancelActiveKeyboardLockBeforeFullscreen
#else
#define MAYBE_CancelActiveKeyboardLockBeforeFullscreen \
  CancelActiveKeyboardLockBeforeFullscreen
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_CancelActiveKeyboardLockBeforeFullscreen) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  // Requesting keyboard lock does not engage until tab-initiated fullscreen.
  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Cancel keyboard lock before fullscreen.
  ASSERT_TRUE(CancelKeyboardLock());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(IsKeyboardLockActive());

  // Tab-initiated fullscreen (JS API) does not engage keyboard lock.
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_FALSE(IsInBrowserFullscreen());
  ASSERT_TRUE(IsActiveTabFullscreen());
  ASSERT_FALSE(IsKeyboardLockActive());

  // New Tab shortcut is no longer prevented.
  int initial_tab_count = GetTabCount();
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_EQ(initial_tab_count + 1, GetTabCount());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       PressEscapeExitsFullscreenWhenEscNotLocked) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  // Do not disable prevent default behavior.  This ensures a webpage cannot
  // prevent the user from exiting fullscreen.

  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  // Single escape key press does exit fullscreen.
  ASSERT_NO_FATAL_FAILURE(SendEscape());
  ASSERT_FALSE(IsActiveTabFullscreen());
  ASSERT_FALSE(IsKeyboardLockActive());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// BringBrowserWindowToFront hangs on Linux: http://crbug.com/163931
#define MAYBE_GainAndLoseFocusInWindowMode DISABLED_GainAndLoseFocusInWindowMode
#else
#define MAYBE_GainAndLoseFocusInWindowMode GainAndLoseFocusInWindowMode
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_GainAndLoseFocusInWindowMode) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  Browser* first_instance = GetActiveBrowser();
  Browser* second_instance = CreateNewBrowserInstance();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));

  // Save this off for querying later as ActiveWebContents() is based on focus
  // and we want to check the state of the web contents associated with the
  // first browser instance.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  // We expect the keyboard lock request to remain valid while the window gains
  // and loses focus, keyboard lock will remain inactive since the initial
  // window is never put into fullscreen.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(content::GetKeyboardLockWidget(web_contents));
  ASSERT_FALSE(IsKeyboardLockActive());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// BringBrowserWindowToFront hangs on Linux: http://crbug.com/163931
#define MAYBE_GainAndLoseFocusInFullscreen DISABLED_GainAndLoseFocusInFullscreen
#else
#define MAYBE_GainAndLoseFocusInFullscreen GainAndLoseFocusInFullscreen
#endif
IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       MAYBE_GainAndLoseFocusInFullscreen) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  // Create a second browser instance so we can switch back and forth between
  // the two instances to simulate focus loss/gain.
  Browser* first_instance = GetActiveBrowser();
  Browser* second_instance = CreateNewBrowserInstance();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));

  // Save this off for querying later as ActiveWebContents() based on focus.
  content::RenderWidgetHostView* first_instance_host_view =
      GetActiveWebContents()->GetRenderWidgetHostView();
  ASSERT_TRUE(first_instance_host_view);

  ASSERT_TRUE(RequestKeyboardLock());
  ASSERT_TRUE(IsKeyboardLockRequestRegistered());
  ASSERT_FALSE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(first_instance_host_view->IsKeyboardLocked());

  // Now we use the test utility libraries to switch between the first and
  // second browser instances.  The expectation is that keyboard lock will be
  // disengaged when the second instance is brought to the foreground and is
  // re-activated when the first instance is given focus.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_FALSE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_FALSE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_FALSE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_instance));
  ASSERT_FALSE(first_instance_host_view->IsKeyboardLocked());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_instance));
  ASSERT_TRUE(first_instance_host_view->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       KeyboardUnlockedWhenNavigatingToSameUrl) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURLWithDisposition(
      GetActiveBrowser(),
      GetEmbeddedTestServer()->GetURL(GetFullscreenFramePath()),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_FALSE(IsKeyboardLockActive());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       KeyboardUnlockedWhenNavigatingAway) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());

  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURLWithDisposition(
      GetActiveBrowser(), GetEmbeddedTestServer()->GetURL(kSimplePageHTML),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_FALSE(IsKeyboardLockActive());
  ASSERT_FALSE(IsKeyboardLockRequestRegistered());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockInteractiveBrowserTest,
                       DownloadNavigationDoesNotUnlock) {
  ASSERT_NO_FATAL_FAILURE(StartFullscreenLockPage());
  ASSERT_TRUE(DisablePreventDefaultOnTestPage());

  ASSERT_TRUE(RequestKeyboardLock(/*lock_all_keys=*/false));
  ASSERT_NO_FATAL_FAILURE(SendJsFullscreenShortcutAndWait());
  ASSERT_TRUE(IsKeyboardLockActive());

  GURL download_url =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII(kDownloadFolder),
                                base::FilePath().AppendASCII(kDownloadFile));
  ui_test_utils::DownloadURL(browser(), download_url);

  ASSERT_TRUE(IsKeyboardLockActive());
#if BUILDFLAG(IS_MAC)
  // Must exit fullscreen before ending the test to prevent crashing while
  // tearing down the test browser, due to the download bubble being shown on
  // changing the fullscreen state while the browser is being destroyed.
  CancelKeyboardLock();
  ExitFullscreen();
#endif
}
