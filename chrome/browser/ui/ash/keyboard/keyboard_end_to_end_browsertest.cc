// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

class KeyboardVisibleWaiter : public ChromeKeyboardControllerClient::Observer {
 public:
  explicit KeyboardVisibleWaiter(bool visible) : visible_(visible) {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }
  ~KeyboardVisibleWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardVisibilityChanged(bool visible) override {
    if (visible == visible_)
      run_loop_.QuitWhenIdle();
  }

 private:
  base::RunLoop run_loop_;
  const bool visible_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardVisibleWaiter);
};  // namespace

bool WaitUntilShown() {
  if (ChromeKeyboardControllerClient::Get()->is_keyboard_visible()) {
    base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
    return true;
  }
  KeyboardVisibleWaiter(true).Wait();
  return ChromeKeyboardControllerClient::Get()->is_keyboard_visible();
}

bool WaitUntilHidden() {
  if (!ChromeKeyboardControllerClient::Get()->is_keyboard_visible()) {
    base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
    return true;
  }
  KeyboardVisibleWaiter(false).Wait();
  return !ChromeKeyboardControllerClient::Get()->is_keyboard_visible();
}

gfx::Size GetScreenBounds() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
}

}  // namespace

class KeyboardEndToEndTest : public InProcessBrowserTest {
 public:
  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

  void SetUpOnMainThread() override {
    GURL test_url = ui_test_utils::GetTestUrl(
        base::FilePath("chromeos/virtual_keyboard"), test_file_);
    ui_test_utils::NavigateToURL(browser(), test_url);
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents_);

    base::RunLoop().RunUntilIdle();

    auto* client = ChromeKeyboardControllerClient::Get();
    ASSERT_TRUE(client);
    ASSERT_TRUE(client->is_keyboard_enabled());
    EXPECT_FALSE(client->is_keyboard_visible());
  }

 protected:
  // Initialized in |SetUpOnMainThread|.
  content::WebContents* web_contents_;

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
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest, ClickBodyHidesKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  content::SimulateMouseClickAt(
      web_contents_, 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(0, 0));
  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputTypeToTextDoesNotHideKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents_,
                      "document.getElementById('username').type = 'password'")
          .error.empty());

  base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
  EXPECT_TRUE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputTypeToNonTextHidesKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents_,
                      "document.getElementById('username').type = 'submit'")
          .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputToReadOnlyHidesKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(
      content::EvalJs(web_contents_,
                      "document.getElementById('username').readOnly = true")
          .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputModeToNumericDoesNotHideKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents_,
                              "document.getElementById('username')."
                              "setAttribute('inputmode', 'numeric')")
                  .error.empty());

  base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
  EXPECT_TRUE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest,
                       ChangeInputModeToNoneHidesKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents_,
                              "document.getElementById('username')."
                              "setAttribute('inputmode', 'none')")
                  .error.empty());

  ASSERT_TRUE(WaitUntilHidden());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFormTest, DeleteInputHidesKeyboard) {
  ClickElementWithId(web_contents_, "username");
  ASSERT_TRUE(WaitUntilShown());

  ASSERT_TRUE(content::EvalJs(web_contents_,
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
      content::EvalJs(web_contents_, "document.getElementById('text').focus()")
          .error.empty());

  base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
  EXPECT_FALSE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndFocusTest,
                       TriggerInputFocusFromUserGestureShowsKeyboard) {
  ClickElementWithId(web_contents_, "sync");

  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureDoesNotShowKeyboard) {
  ClickElementWithId(web_contents_, "async");

  base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
  EXPECT_FALSE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureAfterBlurShowsKeyboard) {
  // If async focus occurs quickly after blur, then it should still invoke the
  // keyboard.
  ClickElementWithId(web_contents_, "text");
  ASSERT_TRUE(WaitUntilShown());

  ClickElementWithId(web_contents_, "blur");
  ASSERT_TRUE(WaitUntilHidden());

  ClickElementWithId(web_contents_, "async");
  ASSERT_TRUE(WaitUntilShown());
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndFocusTest,
    TriggerAsyncInputFocusFromUserGestureAfterBlurTimeoutDoesNotShowKeyboard) {
  ClickElementWithId(web_contents_, "text");
  ASSERT_TRUE(WaitUntilShown());

  ClickElementWithId(web_contents_, "blur");
  ASSERT_TRUE(WaitUntilHidden());

  // Wait until the transient blur threshold (3500ms) passes.
  // TODO(https://crbug.com/849995): Find a way to accelerate the clock without
  // actually waiting in real time.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(3501));

  ClickElementWithId(web_contents_, "async");
  base::RunLoop().RunUntilIdle();  // Allow async operations to complete.
  EXPECT_FALSE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

class KeyboardEndToEndOverscrollTest : public KeyboardEndToEndTest {
 public:
  KeyboardEndToEndOverscrollTest()
      : KeyboardEndToEndTest(base::FilePath("form.html")) {}
  ~KeyboardEndToEndOverscrollTest() override {}

  void FocusAndShowKeyboard() { ClickElementWithId(web_contents_, "username"); }

  void HideKeyboard() {
    auto* controller = ChromeKeyboardControllerClient::Get();
    controller->HideKeyboard(ash::HideReason::kUser);
  }

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

  const int old_height = GetViewportHeight(web_contents_);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(GetViewportHeight(web_contents_), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnNonOverlappingWindowDoesNotAffectViewport) {
  // Set the window bounds so that it does not overlap with the keyboard.
  // The virtual keyboard takes up no more than half the screen height.
  gfx::Size screen_bounds = GetScreenBounds();
  browser()->window()->SetBounds(
      gfx::Rect(0, 0, screen_bounds.width(), screen_bounds.height() / 2));

  const int old_height = GetViewportHeight(web_contents_);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnShortOverlappingWindowMovesWindowUpwards) {
  // Shift the window down so that it overlaps with the keyboard, but shrink the
  // window size so that when it moves upwards, it will no longer overlap with
  // the keyboard.
  gfx::Size screen_bounds = GetScreenBounds();
  browser()->window()->SetBounds(gfx::Rect(0, screen_bounds.height() / 2,
                                           screen_bounds.width(),
                                           screen_bounds.height() / 2));
  const auto old_browser_bounds = browser()->window()->GetBounds();
  const int old_height = GetViewportHeight(web_contents_);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(browser()->window()->GetBounds().y(), old_browser_bounds.y());
  EXPECT_EQ(browser()->window()->GetBounds().height(),
            old_browser_bounds.height());
  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(browser()->window()->GetBounds(), old_browser_bounds);
  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);
}

IN_PROC_BROWSER_TEST_F(
    KeyboardEndToEndOverscrollTest,
    ToggleKeyboardOnTallOverlappingWindowMovesWindowUpwardsAndAffectsViewport) {
  // Shift the window down so that it overlaps with the keyboard, and expand the
  // window size so that when it moves upwards, it will still overlap with
  // the keyboard.
  gfx::Size screen_bounds = GetScreenBounds();
  browser()->window()->SetBounds(gfx::Rect(0, screen_bounds.height() / 3,
                                           screen_bounds.width(),
                                           screen_bounds.height() / 3 * 2));
  const auto old_browser_bounds = browser()->window()->GetBounds();
  const int old_height = GetViewportHeight(web_contents_);

  FocusAndShowKeyboard();
  ASSERT_TRUE(WaitUntilShown());

  EXPECT_LT(browser()->window()->GetBounds().y(), old_browser_bounds.y());
  EXPECT_EQ(browser()->window()->GetBounds().height(),
            old_browser_bounds.height());
  EXPECT_LT(GetViewportHeight(web_contents_), old_height);

  HideKeyboard();
  ASSERT_TRUE(WaitUntilHidden());

  EXPECT_EQ(browser()->window()->GetBounds(), old_browser_bounds);
  EXPECT_EQ(GetViewportHeight(web_contents_), old_height);
}
