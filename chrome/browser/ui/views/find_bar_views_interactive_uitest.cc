// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using content::WebContents;
using ui_test_utils::IsViewFocused;

namespace {
const char kSimplePage[] = "/find_in_page/simple.html";

class WebContentsFocusChangedWatcher : public content::WebContentsObserver {
 public:
  explicit WebContentsFocusChangedWatcher(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    EXPECT_TRUE(web_contents != nullptr);
  }
  ~WebContentsFocusChangedWatcher() override {}

  // Waits until focus changes in the page.
  void Wait() { run_loop_.Run(); }

 private:
  // Overridden WebContentsObserver methods.
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsFocusChangedWatcher);
};
}  // namespace

class FindInPageTest : public InProcessBrowserTest {
 public:
  FindInPageTest() {
    FindBarHost::disable_animations_during_testing_ = true;
  }

  FindBarHost* GetFindBarHost() {
    FindBar* find_bar = browser()->GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  FindBarView* GetFindBarView() { return GetFindBarHost()->find_bar_view(); }

  base::string16 GetFindBarText() { return GetFindBarHost()->GetFindText(); }

  base::string16 GetFindBarSelectedText() {
    return GetFindBarHost()->GetFindBarTesting()->GetFindSelectedText();
  }

  bool IsFindBarVisible() { return GetFindBarHost()->IsFindBarVisible(); }

  void ClickOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    view->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                        gfx::Point(), base::TimeTicks(),
                                        ui::EF_LEFT_MOUSE_BUTTON,
                                        ui::EF_LEFT_MOUSE_BUTTON));
    view->OnMouseReleased(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                         gfx::Point(), base::TimeTicks(),
                                         ui::EF_LEFT_MOUSE_BUTTON,
                                         ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TapOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    view->OnGestureEvent(&event);
  }

  FindNotificationDetails WaitForFindResult() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::FindResultWaiter(web_contents).Wait();
    return FindTabHelper::FromWebContents(web_contents)->find_result();
  }

  FindNotificationDetails WaitForFinalFindResult() {
    while (true) {
      auto details = WaitForFindResult();
      if (details.final_update())
        return details;
    }
  }

  bool SendKeyPressAndWait(const Browser* browser,
                           ui::KeyboardCode key,
                           bool control,
                           bool shift,
                           bool alt,
                           bool command,
                           int type,
                           const content::NotificationSource& source) {
    content::WindowedNotificationObserver observer(type, source);

    if (!ui_test_utils::SendKeyPressSync(browser, key, control, shift, alt,
                                         command))
      return false;

    observer.Wait();
    return !testing::Test::HasFatalFailure();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FindInPageTest);
};

// Flaky because the test server fails to start? See: http://crbug.com/96594.
IN_PROC_BROWSER_TEST_F(FindInPageTest, CrashEscHandlers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());

  // Open another tab (tab B).
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Select tab A.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});

  // Close tab B.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabStripModel::CLOSE_NONE);

  // Click on the location bar so that Find box loses focus.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_OMNIBOX));
  // Check the location bar is focused.
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // This used to crash until bug 1303709 was fixed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, NavigationByKeyEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("a"),
      true, false, NULL, NULL);

  // The previous button should still be focused after pressing [Enter] on it.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));

  // The next button should still be focused after pressing [Enter] on it.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("b"),
      true, false, NULL, NULL);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, ButtonsDoNotAlterFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  const int match_count = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("e"),
      true, false, nullptr, nullptr);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // This test requires at least 3 possible matches.
  ASSERT_GE(match_count, 3);
  // Avoid GetViewByID on BrowserView; the find bar is outside its hierarchy.
  FindBarView* find_bar_view = GetFindBarView();
  views::View* next_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  views::View* previous_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);

  // Clicking the next and previous buttons should not alter the focused view.
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ClickOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Tapping the next and previous buttons should not alter the focused view.
  TapOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  TapOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The same should be true even when the previous button is focused.
  previous_button->RequestFocus();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  TapOnView(next_button);
  EXPECT_EQ(3, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, ButtonsDisabledWithoutText) {
  if (browser()
          ->GetFindBarController()
          ->find_bar()
          ->HasGlobalFindPasteboard()) {
    // The presence of a global find pasteboard does not guarantee the find bar
    // will be empty on launch.
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The buttons should be disabled as there is no text entered in the find bar
  // and no search has been issued yet.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Focus the location bar, open and close the find-in-page, focus should
  // return to the location bar.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // Ensure the creation of the find bar controller.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindOnPageSelectionAction::kKeep, FindBoxResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Focus the location bar, find something on the page, close the find box,
  // focus should go to the page.
  chrome::FocusLocationBar(browser());
  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("a"), true, false, NULL, NULL);
  browser()->GetFindBarController()->EndFindSession(
      FindOnPageSelectionAction::kKeep, FindBoxResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the location bar, open and close the find box, focus should return to
  // the location bar (same as before, just checking that http://crbug.com/23599
  // is fixed).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindOnPageSelectionAction::kKeep, FindBoxResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

// Flaky on Windows. https://crbug.com/792313
#if defined(OS_WIN)
#define MAYBE_SelectionRestoreOnTabSwitch DISABLED_SelectionRestoreOnTabSwitch
#else
#define MAYBE_SelectionRestoreOnTabSwitch SelectionRestoreOnTabSwitch
#endif
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_SelectionRestoreOnTabSwitch) {
  // Mac intentionally changes selection on focus.
  if (views::PlatformStyle::kTextfieldScrollsToStartOnFocusChange)
    return;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page in the current tab (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "abc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_B, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("abc"), GetFindBarText());

  // Select "bc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  EXPECT_EQ(ASCIIToUTF16("bc"), GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "def".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_E, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("def"), GetFindBarText());

  // Select "de".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_HOME, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  EXPECT_EQ(ASCIIToUTF16("de"), GetFindBarSelectedText());

  // Select tab A. Find bar should select "bc".
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(ASCIIToUTF16("bc"), GetFindBarSelectedText());

  // Select tab B. Find bar should select "de".
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(ASCIIToUTF16("de"), GetFindBarSelectedText());
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestoreOnTabSwitch) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'a'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("a"), true, false, NULL, NULL);
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Make sure Find box is open.
  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'b'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("b"), true, false, NULL, NULL);
  // Mac intentionally changes selection on focus.
  if (!views::PlatformStyle::kTextfieldScrollsToStartOnFocusChange)
    EXPECT_EQ(ASCIIToUTF16("b"), GetFindBarSelectedText());

  // Set focus away from the Find bar (to the Location bar).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Select tab A. Find bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  if (!views::PlatformStyle::kTextfieldScrollsToStartOnFocusChange)
    EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Select tab B. Location bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

// FindInPage on Mac doesn't use prepopulated values. Search there is global.
#if !defined(OS_MACOSX) && !defined(USE_AURA)
// Flaky because the test server fails to start? See: http://crbug.com/96594.
// This tests that whenever you clear values from the Find box and close it that
// it respects that and doesn't show you the last search, as reported in bug:
// http://crbug.com/40121. For Aura see bug http://crbug.com/292299.
IN_PROC_BROWSER_TEST_F(FindInPageTest, PrepopulateRespectBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarText());

  // Delete "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BACK, false, false, false, false));

  // Validate we have cleared the text.
  EXPECT_EQ(base::string16(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  EXPECT_EQ(base::string16(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Press F3 to trigger FindNext.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F3, false, false, false, false));

  // After the Find box has been reopened, it should still have no prepopulate
  // value.
  EXPECT_EQ(base::string16(), GetFindBarText());
}
#endif

// Flaky on Win. http://crbug.com/92467
// Flaky on ChromeOS. http://crbug.com/118216
// Flaky on linux aura. http://crbug.com/163931
#if defined(TOOLKIT_VIEWS)
#define MAYBE_PasteWithoutTextChange DISABLED_PasteWithoutTextChange
#else
#define MAYBE_PasteWithoutTextChange PasteWithoutTextChange
#endif

IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_PasteWithoutTextChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarText());

  // Reload the page to clear the matching result.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Focus the Find bar again to make sure the text is selected.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // "a" should be selected.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Press Ctrl-C to copy the content.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, true, false, false, false));

  base::string16 str;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &str);

  // Make sure the text is copied successfully.
  EXPECT_EQ(ASCIIToUTF16("a"), str);

  // Press Ctrl-V to paste the content back, it should start finding even if the
  // content is not changed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_V, true, false, false, false));
  FindNotificationDetails details = WaitForFindResult();
  EXPECT_TRUE(details.number_of_matches() > 0);
}

// Slow flakiness on Linux. crbug.com/803743
#if defined(OS_LINUX)
#define MAYBE_CtrlEnter DISABLED_CtrlEnter
#else
#define MAYBE_CtrlEnter CtrlEnter
#endif
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_CtrlEnter) {
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html,This is some text with a "
                                    "<a href=\"about:blank\">link</a>."));

  browser()->GetFindBarController()->Show();

  // Search for "link".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("link"), GetFindBarText());

  ui_test_utils::UrlLoadObserver observer(
      GURL("about:blank"), content::NotificationService::AllSources());

  // Send Ctrl-Enter, should cause navigation to about:blank.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, true, false, false, false));

  observer.Wait();
}

// FindInPage on Mac doesn't use prepopulated values. Search there is global.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(FindInPageTest, SelectionDuringFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/find_in_page/find_from_selection.html"));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WebContentsFocusChangedWatcher watcher(web_contents);

  // Tab to the input (which selects the text inside)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));

  watcher.Wait();

  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // verify the text matches the selection
  EXPECT_EQ(ASCIIToUTF16("text"), GetFindBarText());
  FindNotificationDetails details = WaitForFindResult();
  EXPECT_TRUE(details.number_of_matches() > 0);
}
#endif

IN_PROC_BROWSER_TEST_F(FindInPageTest, GlobalEscapeClosesFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));

  // Open find
  browser()->GetFindBarController()->Show(false, true);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Put focus into location bar
  chrome::FocusLocationBar(browser());

  // Close find with escape
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Find should be closed
  ASSERT_FALSE(IsFindBarVisible());
}
