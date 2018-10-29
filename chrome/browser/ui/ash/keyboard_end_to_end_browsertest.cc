// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_resource_util.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace keyboard {

class KeyboardEndToEndTest : public InProcessBrowserTest {
 public:
  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableVirtualKeyboard);
  }

  void SetUpOnMainThread() override {
    GURL test_url = ui_test_utils::GetTestUrl(
        base::FilePath("chromeos/virtual_keyboard"), test_file_);
    ui_test_utils::NavigateToURL(browser(), test_url);
    web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    ASSERT_TRUE(KeyboardController::Get());
    ASSERT_TRUE(KeyboardController::Get()->IsEnabled());
    EXPECT_FALSE(IsKeyboardVisible());
  }

 protected:
  bool IsKeyboardVisible() {
    auto* keyboard_controller = keyboard::KeyboardController::Get();
    return keyboard_controller->IsKeyboardVisible();
  }

  // Initialized in |SetUpOnMainThread|.
  content::WebContents* web_contents;

  explicit KeyboardEndToEndTest(const base::FilePath& test_file)
      : test_file_(test_file) {}
  ~KeyboardEndToEndTest() override {}

  // Get the value of the attribute attribute |attribute| on the DOM element
  // with the given |id|.
  std::string GetElementAttribute(content::WebContents* web_contents,
                                  const std::string& id,
                                  const std::string& attribute) {
    return content::EvalJs(web_contents,
                           content::JsReplace(
                               "document.getElementById($1).getAttribute($2)",
                               id, attribute))
        .ExtractString();
  }

  // Simulates a click on the middle of the DOM element with the given |id|.
  void ClickElementWithId(content::WebContents* web_contents,
                          const std::string& id) {
    // Get the center coordinates of the DOM element.
    const int x =
        content::EvalJs(
            web_contents,
            content::JsReplace("const bounds = "
                               "document.getElementById($1)."
                               "getBoundingClientRect();"
                               "Math.floor(bounds.left + bounds.width / 2)",
                               id))
            .ExtractInt();
    const int y =
        content::EvalJs(
            web_contents,
            content::JsReplace("const bounds = "
                               "document.getElementById($1)."
                               "getBoundingClientRect();"
                               "Math.floor(bounds.top + bounds.height / 2)",
                               id))
            .ExtractInt();

    content::SimulateMouseClickAt(
        web_contents, 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(x, y));
  }

 private:
  base::FilePath test_file_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardEndToEndTest);
};

class KeyboardEndToEndFormTest : public KeyboardEndToEndTest {
 public:
  KeyboardEndToEndFormTest()
      : KeyboardEndToEndTest(base::FilePath("form.html")) {}
  ~KeyboardEndToEndFormTest() override {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(KeyboardEndToEndFormTest);
};

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest, ClickTextFieldShowsKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest, ClickBodyHidesKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  content::SimulateMouseClickAt(
      web_contents, 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(0, 0));
  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputTypeToTextDoesNotHideKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents,
                      "document.getElementById('username').type = 'password'")
          .error.empty());

  EXPECT_FALSE(IsKeyboardHiding());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputTypeToNonTextHidesKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents,
                      "document.getElementById('username').type = 'submit'")
          .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputToReadOnlyHidesKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents,
                      "document.getElementById('username').readOnly = true")
          .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputModeToNumericDoesNotHideKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents,
                              "document.getElementById('username')."
                              "setAttribute('inputmode', 'numeric')")
                  .error.empty());

  EXPECT_FALSE(IsKeyboardHiding());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputModeToNoneHidesKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents,
                              "document.getElementById('username')."
                              "setAttribute('inputmode', 'none')")
                  .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest, DeleteInputHidesKeyboard) {
  ClickElementWithId(web_contents, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents,
                              "document.getElementById('username').remove()")
                  .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

class KeyboardEndToEndFocusTest : public KeyboardEndToEndTest {
 public:
  KeyboardEndToEndFocusTest()
      : KeyboardEndToEndTest(base::FilePath("focus.html")) {}
  ~KeyboardEndToEndFocusTest() override {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(KeyboardEndToEndFocusTest);
};

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFocusTest,
                       TriggerInputFocusWithoutUserGestureDoesNotShowKeyboard) {
  ASSERT_TRUE(
      content::EvalJs(web_contents, "document.getElementById('text').focus()")
          .error.empty());

  EXPECT_FALSE(IsKeyboardShowing());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFocusTest,
                       TriggerInputFocusFromUserGestureShowsKeyboard) {
  ClickElementWithId(web_contents, "sync");

  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureDoesNotShowKeyboard) {
  ClickElementWithId(web_contents, "async");

  EXPECT_FALSE(IsKeyboardShowing());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureAfterBlurShowsKeyboard) {
  // If async focus occurs quickly after blur, then it should still invoke the
  // keyboard.
  ClickElementWithId(web_contents, "text");
  ASSERT_TRUE(WaitUntilShown());

  ClickElementWithId(web_contents, "blur");
  ASSERT_TRUE(WaitUntilHidden());

  ClickElementWithId(web_contents, "async");
  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureAfterBlurTimeoutDoesNotShowKeyboard) {
  ClickElementWithId(web_contents, "text");
  ASSERT_TRUE(WaitUntilShown());

  ClickElementWithId(web_contents, "blur");
  ASSERT_TRUE(WaitUntilHidden());

  // Wait until the transient blur threshold (3500ms) passes.
  // TODO(https://crbug.com/849995): Find a way to accelerate the clock without
  // actually waiting in real time.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(3501));

  ClickElementWithId(web_contents, "async");
  EXPECT_FALSE(IsKeyboardShowing());
}

class KeyboardEndToEndOverscrollTest : public KeyboardEndToEndTest {
 public:
  KeyboardEndToEndOverscrollTest()
      : KeyboardEndToEndTest(base::FilePath("form.html")) {}
  ~KeyboardEndToEndOverscrollTest() override {}

  void FocusAndShowKeyboard() { ClickElementWithId(web_contents, "username"); }

  void HideKeyboard() { KeyboardController::Get()->HideKeyboardByUser(); }

 protected:
  int GetViewportHeight(content::WebContents* web_contents) {
    return web_contents->GetRenderWidgetHostView()
        ->GetVisibleViewportSize()
        .height();
  }

  DISALLOW_COPY_AND_ASSIGN(KeyboardEndToEndOverscrollTest);
};

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndOverscrollTest,
                       ToggleKeyboardOnMaximizedWindowAffectsViewport) {
  browser()->window()->Maximize();
  aura::test::WaitForAllChangesToComplete();

  const int old_height = GetViewportHeight(web_contents);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(GetViewportHeight(web_contents), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(GetViewportHeight(web_contents), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnNonOverlappingWindowDoesNotAffectViewport) {
  // Set the window bounds so that it does not overlap with the keyboard.
  // The virtual keyboard takes up no more than half the screen height.
  const auto screen_bounds = ash::Shell::GetPrimaryRootWindow()->bounds();
  browser()->window()->SetBounds(
      gfx::Rect(0, 0, screen_bounds.width(), screen_bounds.height() / 2));
  aura::test::WaitForAllChangesToComplete();

  const int old_height = GetViewportHeight(web_contents);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_EQ(GetViewportHeight(web_contents), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(GetViewportHeight(web_contents), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnShortOverlappingWindowMovesWindowUpwards) {
  // Shift the window down so that it overlaps with the keyboard, but shrink the
  // window size so that when it moves upwards, it will no longer overlap with
  // the keyboard.
  const auto screen_bounds = ash::Shell::GetPrimaryRootWindow()->bounds();
  browser()->window()->SetBounds(gfx::Rect(0, screen_bounds.height() / 2,
                                           screen_bounds.width(),
                                           screen_bounds.height() / 2));
  aura::test::WaitForAllChangesToComplete();

  const auto old_browser_bounds = browser()->window()->GetBounds();
  const int old_height = GetViewportHeight(web_contents);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(browser()->window()->GetBounds().y(), old_browser_bounds.y());
  EXPECT_EQ(browser()->window()->GetBounds().height(),
            old_browser_bounds.height());
  EXPECT_EQ(GetViewportHeight(web_contents), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(browser()->window()->GetBounds(), old_browser_bounds);
  EXPECT_EQ(GetViewportHeight(web_contents), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnTallOverlappingWindowMovesWindowUpwardsAndAffectsViewport) {
  // Shift the window down so that it overlaps with the keyboard, and expand the
  // window size so that when it moves upwards, it will still overlap with
  // the keyboard.
  const auto screen_bounds = ash::Shell::GetPrimaryRootWindow()->bounds();
  browser()->window()->SetBounds(gfx::Rect(0, screen_bounds.height() / 3,
                                           screen_bounds.width(),
                                           screen_bounds.height() / 3 * 2));
  aura::test::WaitForAllChangesToComplete();

  const auto old_browser_bounds = browser()->window()->GetBounds();
  const int old_height = GetViewportHeight(web_contents);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(browser()->window()->GetBounds().y(), old_browser_bounds.y());
  EXPECT_EQ(browser()->window()->GetBounds().height(),
            old_browser_bounds.height());
  EXPECT_LT(GetViewportHeight(web_contents), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(browser()->window()->GetBounds(), old_browser_bounds);
  EXPECT_EQ(GetViewportHeight(web_contents), old_height);
}

}  // namespace keyboard
