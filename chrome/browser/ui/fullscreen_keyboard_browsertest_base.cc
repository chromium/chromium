// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_keyboard_browsertest_base.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace {

// The html file to receive key events, prevent defaults and export all the
// events with "getKeyEventReport()" function. It has two magic keys: pressing
// "S" to enter fullscreen mode; pressing "X" to indicate the end of all the
// keys (see FinishTestAndVerifyResult() function).
constexpr char kFullscreenKeyboardLockHTML[] =
    "/fullscreen_keyboardlock/fullscreen_keyboardlock.html";

// On MacOSX command key is used for most of the shortcuts, so replace it with
// control to reduce the complexity of comparison of the results.
void NormalizeMetaKeyForMacOS(std::string* output) {
#if defined(OS_MACOSX)
  base::ReplaceSubstringsAfterOffset(output, 0, "MetaLeft", "ControlLeft");
#endif
}

}  // namespace

FullscreenKeyboardBrowserTestBase::FullscreenKeyboardBrowserTestBase() =
    default;

FullscreenKeyboardBrowserTestBase::~FullscreenKeyboardBrowserTestBase() =
    default;

net::EmbeddedTestServer*
FullscreenKeyboardBrowserTestBase::GetEmbeddedTestServer() {
  return embedded_test_server();
}

bool FullscreenKeyboardBrowserTestBase::IsActiveTabFullscreen() const {
  auto* contents = GetActiveWebContents();
  return contents->GetDelegate()->IsFullscreenForTabOrPending(contents);
}

bool FullscreenKeyboardBrowserTestBase::IsInBrowserFullscreen() const {
  return GetActiveBrowser()
      ->exclusive_access_manager()
      ->fullscreen_controller()
      ->IsFullscreenForBrowser();
}

content::WebContents* FullscreenKeyboardBrowserTestBase::GetActiveWebContents()
    const {
  return GetActiveBrowser()->tab_strip_model()->GetActiveWebContents();
}

int FullscreenKeyboardBrowserTestBase::GetActiveTabIndex() const {
  return GetActiveBrowser()->tab_strip_model()->active_index();
}

int FullscreenKeyboardBrowserTestBase::GetTabCount() const {
  return GetActiveBrowser()->tab_strip_model()->count();
}

size_t FullscreenKeyboardBrowserTestBase::GetBrowserCount() const {
  return BrowserList::GetInstance()->size();
}

Browser* FullscreenKeyboardBrowserTestBase::GetActiveBrowser() const {
  return BrowserList::GetInstance()->GetLastActive();
}

Browser* FullscreenKeyboardBrowserTestBase::CreateNewBrowserInstance() {
  Browser* first_instance = GetActiveBrowser();
  const size_t initial_browser_count = GetBrowserCount();
  EXPECT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_N));
  WaitForBrowserCount(initial_browser_count + 1);
  Browser* second_instance = GetActiveBrowser();
  EXPECT_NE(first_instance, second_instance);

  return second_instance;
}

void FullscreenKeyboardBrowserTestBase::FocusOnLastActiveBrowser() {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(GetActiveBrowser()));
}

void FullscreenKeyboardBrowserTestBase::WaitForBrowserCount(size_t expected) {
  while (GetBrowserCount() != expected)
    base::RunLoop().RunUntilIdle();
}

void FullscreenKeyboardBrowserTestBase::WaitForTabCount(int expected) {
  while (GetTabCount() != expected)
    base::RunLoop().RunUntilIdle();
}

void FullscreenKeyboardBrowserTestBase::WaitForActiveTabIndex(int expected) {
  while (GetActiveTabIndex() != expected)
    base::RunLoop().RunUntilIdle();
}

void FullscreenKeyboardBrowserTestBase::WaitForInactiveTabIndex(int expected) {
  while (GetActiveTabIndex() == expected)
    base::RunLoop().RunUntilIdle();
}

void FullscreenKeyboardBrowserTestBase::StartFullscreenLockPage() {
  // Ensures the initial states.
  ASSERT_EQ(1, GetTabCount());
  ASSERT_EQ(0, GetActiveTabIndex());
  ASSERT_EQ(1U, GetBrowserCount());
  // Add a second tab for counting and focus purposes.
  AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, GetTabCount());
  ASSERT_EQ(1U, GetBrowserCount());

  if (!GetEmbeddedTestServer()->Started())
    ASSERT_TRUE(GetEmbeddedTestServer()->Start());
  ui_test_utils::NavigateToURLWithDisposition(
      GetActiveBrowser(),
      GetEmbeddedTestServer()->GetURL(kFullscreenKeyboardLockHTML),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

void FullscreenKeyboardBrowserTestBase::SendShortcut(ui::KeyboardCode key,
                                                     bool shift /* = false */) {
#if defined(OS_MACOSX)
  const bool control_modifier = false;
  const bool command_modifier = true;
#else
  const bool control_modifier = true;
  const bool command_modifier = false;
#endif
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), key,
                                              control_modifier, shift, false,
                                              command_modifier));

  expected_result_ += ui::KeycodeConverter::DomCodeToCodeString(
      ui::UsLayoutKeyboardCodeToDomCode(key));
  expected_result_ += " ctrl:";
  expected_result_ += control_modifier ? "true" : "false";
  expected_result_ += " shift:";
  expected_result_ += shift ? "true" : "false";
  expected_result_ += " alt:false";
  expected_result_ += " meta:";
  expected_result_ += command_modifier ? "true" : "false";
  expected_result_ += '\n';
}

void FullscreenKeyboardBrowserTestBase::SendShiftShortcut(
    ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(SendShortcut(key, true));
}

void FullscreenKeyboardBrowserTestBase::SendFullscreenShortcutAndWait() {
  // On MacOSX, entering and exiting fullscreen are not synchronous. So we wait
  // for the observer to notice the change of fullscreen state.
  FullscreenNotificationObserver observer(GetActiveBrowser());
// Enter fullscreen.
#if defined(OS_MACOSX)
  // On MACOSX, Command + Control + F is used.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_F,
                                              true, false, false, true));
#elif defined(OS_CHROMEOS)
  // A dedicated fullscreen key is used on Chrome OS, so send a fullscreen
  // command directly instead, to avoid constructing the key press.
  ASSERT_TRUE(chrome::ExecuteCommand(GetActiveBrowser(), IDC_FULLSCREEN));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_F11,
                                              false, false, false, false));
#endif

// Mac fullscreen is simulated in tests and is performed synchronously with the
// keyboard events. As a result, content doesn't actually know it has entered
// fullscreen. For more details, see ScopedFakeNSWindowFullscreen.
// TODO(crbug.com/837438): Remove this once ScopedFakeNSWindowFullscreen fires
// OnFullscreenStateChanged.
#if !defined(OS_MACOSX)
  observer.Wait();
#endif
}

void FullscreenKeyboardBrowserTestBase::SendJsFullscreenShortcutAndWait() {
  FullscreenNotificationObserver observer(GetActiveBrowser());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_S,
                                              false, false, false, false));
  expected_result_ += "KeyS ctrl:false shift:false alt:false meta:false\n";
  observer.Wait();
  ASSERT_TRUE(IsActiveTabFullscreen());
}

void FullscreenKeyboardBrowserTestBase::SendEscape() {
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      GetActiveBrowser(), ui::VKEY_ESCAPE, false, false, false, false));
  expected_result_ += "Escape ctrl:false shift:false alt:false meta:false\n";
}

void FullscreenKeyboardBrowserTestBase::
    SendEscapeAndWaitForExitingFullscreen() {
  FullscreenNotificationObserver observer(GetActiveBrowser());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      GetActiveBrowser(), ui::VKEY_ESCAPE, false, false, false, false));
  observer.Wait();
  ASSERT_FALSE(IsActiveTabFullscreen());
}

void FullscreenKeyboardBrowserTestBase::SendShortcutsAndExpectPrevented() {
  const int initial_active_index = GetActiveTabIndex();
  const int initial_tab_count = GetTabCount();
  const size_t initial_browser_count = GetBrowserCount();
  // The tab should not be closed.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  ASSERT_EQ(initial_tab_count, GetTabCount());
  // The window should not be closed.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_W));
  ASSERT_EQ(initial_browser_count, GetBrowserCount());
  // A new tab should not be created.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  ASSERT_EQ(initial_tab_count, GetTabCount());
  // A new window should not be created.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_N));
  ASSERT_EQ(initial_browser_count, GetBrowserCount());
  // A new incognito window should not be created.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_N));
  ASSERT_EQ(initial_browser_count, GetBrowserCount());
  // Last closed tab should not be restored.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_T));
  ASSERT_EQ(initial_tab_count, GetTabCount());
  // Browser should not switch to the next tab.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_TAB));
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());
  // Browser should not switch to the previous tab.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_TAB));
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());
}

void FullscreenKeyboardBrowserTestBase::SendShortcutsAndExpectNotPrevented(
    bool js_fullscreen) {
  const int initial_active_index = GetActiveTabIndex();
  const int initial_tab_count = GetTabCount();
  const size_t initial_browser_count = GetBrowserCount();
  const auto enter_fullscreen = [this, js_fullscreen]() {
    ASSERT_TRUE(
        ui_test_utils::BringBrowserWindowToFront(this->GetActiveBrowser()));
    if (js_fullscreen) {
      if (!this->IsActiveTabFullscreen()) {
        static const std::string page =
            "<html><head></head><body></body><script>"
            "document.addEventListener('keydown', "
            "    (e) => {"
            "      if (e.code == 'KeyS') { "
            "        document.body.webkitRequestFullscreen();"
            "      }"
            "    });"
            "</script></html>";
        ui_test_utils::NavigateToURLWithDisposition(
            this->GetActiveBrowser(), GURL("data:text/html," + page),
            WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
        ASSERT_NO_FATAL_FAILURE(this->SendJsFullscreenShortcutAndWait());
      }
    } else {
      if (!this->IsInBrowserFullscreen()) {
        ASSERT_NO_FATAL_FAILURE(this->SendFullscreenShortcutAndWait());
      }
      ASSERT_TRUE(this->IsInBrowserFullscreen());
    }
  };

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // A new tab should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // The newly created tab should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  WaitForTabCount(initial_tab_count);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // A new tab should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // The previous tab should be focused.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_TAB,
                                              true, true, false, false));
  WaitForActiveTabIndex(initial_active_index);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // The newly created tab should be focused.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_TAB,
                                              true, false, false, false));
  WaitForInactiveTabIndex(initial_active_index);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // The newly created tab should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  WaitForTabCount(initial_tab_count);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // A new window should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_N));
  WaitForBrowserCount(initial_browser_count + 1);
  ASSERT_EQ(initial_browser_count + 1, GetBrowserCount());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());

  // The newly created window should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_W));
  WaitForBrowserCount(initial_browser_count);

  ASSERT_EQ(initial_browser_count, GetBrowserCount());
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  ASSERT_NO_FATAL_FAILURE(enter_fullscreen());
}

void FullscreenKeyboardBrowserTestBase::VerifyShortcutsAreNotPrevented() {
  const int initial_active_index = GetActiveTabIndex();
  const int initial_tab_count = GetTabCount();
  const size_t initial_browser_count = GetBrowserCount();

  // A new tab should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  // The newly created tab should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  WaitForTabCount(initial_tab_count);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  // A new tab should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_T));
  WaitForTabCount(initial_tab_count + 1);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  // The previous tab should be focused.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_TAB,
                                              true, true, false, false));
  WaitForActiveTabIndex(initial_active_index);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  // The newly created tab should be focused.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_TAB,
                                              true, false, false, false));
  WaitForInactiveTabIndex(initial_active_index);
  ASSERT_NE(initial_active_index, GetActiveTabIndex());

  // The newly created tab should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_W));
  WaitForTabCount(initial_tab_count);
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());

  // A new window should be created and focused.
  ASSERT_NO_FATAL_FAILURE(SendShortcut(ui::VKEY_N));
  WaitForBrowserCount(initial_browser_count + 1);
  ASSERT_EQ(initial_browser_count + 1, GetBrowserCount());

  // The newly created window should be closed.
  ASSERT_NO_FATAL_FAILURE(SendShiftShortcut(ui::VKEY_W));
  WaitForBrowserCount(initial_browser_count);

  ASSERT_EQ(initial_browser_count, GetBrowserCount());
  ASSERT_EQ(initial_active_index, GetActiveTabIndex());
}

void FullscreenKeyboardBrowserTestBase::FinishTestAndVerifyResult() {
  // The renderer process receives key events through IPC channel,
  // SendKeyPressSync() cannot guarantee the JS has processed the key event it
  // sent. So we sent a KeyX to the webpage to indicate the end of the test
  // case. After processing this key event, web page is safe to send the record
  // back through window.domAutomationController.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(GetActiveBrowser(), ui::VKEY_X,
                                              false, false, false, false));
  expected_result_ += "KeyX ctrl:false shift:false alt:false meta:false";
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), "getKeyEventReport();", &result));
  NormalizeMetaKeyForMacOS(&result);
  NormalizeMetaKeyForMacOS(&expected_result_);
  base::TrimWhitespaceASCII(result, base::TRIM_ALL, &result);
  ASSERT_EQ(expected_result_, result);
}

std::string FullscreenKeyboardBrowserTestBase::GetFullscreenFramePath() {
  return kFullscreenKeyboardLockHTML;
}

void FullscreenKeyboardBrowserTestBase::SetUpOnMainThread() {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(GetActiveBrowser()));
}
