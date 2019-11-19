// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace {

const int kKeyboardHeightForTest = 100;

// TODO(shend): Remove this since all calls are synchronous now.
class KeyboardVisibleWaiter : public ChromeKeyboardControllerClient::Observer {
 public:
  explicit KeyboardVisibleWaiter(bool visible) : visible_(visible) {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }
  ~KeyboardVisibleWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() {
    if (ChromeKeyboardControllerClient::Get()->is_keyboard_visible() ==
        visible_) {
      return;
    }
    run_loop_.Run();
  }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardVisibilityChanged(bool visible) override {
    if (visible == visible_)
      run_loop_.QuitWhenIdle();
  }

 private:
  base::RunLoop run_loop_;
  const bool visible_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardVisibleWaiter);
};

class KeyboardLoadedWaiter : public ChromeKeyboardControllerClient::Observer {
 public:
  KeyboardLoadedWaiter() {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }
  ~KeyboardLoadedWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() {
    if (ChromeKeyboardControllerClient::Get()->is_keyboard_loaded())
      return;
    run_loop_.Run();
  }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardLoaded() override { run_loop_.QuitWhenIdle(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLoadedWaiter);
};

class KeyboardOccludedBoundsChangeWaiter
    : public ChromeKeyboardControllerClient::Observer {
 public:
  KeyboardOccludedBoundsChangeWaiter() {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }
  ~KeyboardOccludedBoundsChangeWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& bounds) override {
    run_loop_.QuitWhenIdle();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOccludedBoundsChangeWaiter);
};

ui::InputMethod* GetInputMethod() {
  aura::Window* root_window = ChromeKeyboardControllerClient::Get()
                                  ->GetKeyboardWindow()
                                  ->GetRootWindow();
  return root_window ? root_window->GetHost()->GetInputMethod() : nullptr;
}

}  // namespace

class KeyboardControllerWebContentTest : public InProcessBrowserTest {
 public:
  KeyboardControllerWebContentTest() {}
  ~KeyboardControllerWebContentTest() override {}

  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 protected:
  void FocusEditableNodeAndShowKeyboard(const gfx::Rect& init_bounds) {
    client =
        std::make_unique<ui::DummyTextInputClient>(ui::TEXT_INPUT_TYPE_TEXT);
    ui::InputMethod* input_method = GetInputMethod();
    ASSERT_TRUE(input_method);
    input_method->SetFocusedTextInputClient(client.get());
    input_method->ShowVirtualKeyboardIfEnabled();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    auto* keyboard_controller = ChromeKeyboardControllerClient::Get();
    keyboard_controller->GetKeyboardWindow()->SetBounds(init_bounds);
  }

  void FocusNonEditableNode() {
    client =
        std::make_unique<ui::DummyTextInputClient>(ui::TEXT_INPUT_TYPE_NONE);
    GetInputMethod()->SetFocusedTextInputClient(client.get());
  }

  void MockEnableIMEInDifferentExtension(const std::string& url,
                                         const gfx::Rect& init_bounds) {
    DCHECK(!url.empty());
    auto* keyboard_controller = ChromeKeyboardControllerClient::Get();
    keyboard_controller->set_virtual_keyboard_url_for_test(GURL(url));
    keyboard_controller->ReloadKeyboardIfNeeded();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    keyboard_controller->GetKeyboardWindow()->SetBounds(init_bounds);
  }

 private:
  std::unique_ptr<ui::DummyTextInputClient> client;
  ui::ScopedTestInputMethodFactory scoped_test_input_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerWebContentTest);
};

// Test for crbug.com/404340. After enabling an IME in a different extension,
// its virtual keyboard should not become visible if previous one is not.
IN_PROC_BROWSER_TEST_F(KeyboardControllerWebContentTest,
                       EnableIMEInDifferentExtension) {
  KeyboardLoadedWaiter().Wait();

  gfx::Rect test_bounds(0, 0, 0, kKeyboardHeightForTest);
  FocusEditableNodeAndShowKeyboard(test_bounds);
  KeyboardVisibleWaiter(true).Wait();

  FocusNonEditableNode();
  KeyboardVisibleWaiter(false).Wait();

  MockEnableIMEInDifferentExtension("chrome-extension://domain-1", test_bounds);
  // Keyboard should not become visible if previous keyboard is not.
  EXPECT_FALSE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());

  FocusEditableNodeAndShowKeyboard(test_bounds);
  // Keyboard should become visible after focus on an editable node.
  KeyboardVisibleWaiter(true).Wait();

  // Simulate hide keyboard by pressing hide key on the virtual keyboard.
  ChromeKeyboardControllerClient::Get()->HideKeyboard(ash::HideReason::kUser);
  KeyboardVisibleWaiter(false).Wait();

  MockEnableIMEInDifferentExtension("chrome-extension://domain-2", test_bounds);
  // Keyboard should not become visible if previous keyboard is not, even if it
  // is currently focused on an editable node.
  EXPECT_FALSE(ChromeKeyboardControllerClient::Get()->is_keyboard_visible());
}

// This test requires using the Ash keyboard window for EventGenerator to work.
// TODO(stevenjb/shend): Investigate/fix.
IN_PROC_BROWSER_TEST_F(KeyboardControllerWebContentTest,
                       CanDragFloatingKeyboardWithMouse) {
  ChromeKeyboardControllerClient::Get()->SetContainerType(
      keyboard::ContainerType::kFloating, base::nullopt, base::DoNothing());

  auto* controller = keyboard::KeyboardUIController::Get();
  controller->ShowKeyboard(false);
  KeyboardVisibleWaiter(true).Wait();

  aura::Window* keyboard_window = controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Point(0, 0), keyboard_window->bounds().origin());

  controller->SetDraggableArea(keyboard_window->bounds());

  // Drag the top left corner of the keyboard to move it.
  ui::test::EventGenerator event_generator(keyboard_window->GetRootWindow());
  event_generator.MoveMouseTo(gfx::Point(0, 0));
  event_generator.PressLeftButton();
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  event_generator.ReleaseLeftButton();
  event_generator.MoveMouseTo(gfx::Point(100, 100));

  EXPECT_EQ(gfx::Point(50, 50), keyboard_window->bounds().origin());
}

class KeyboardControllerAppWindowTest
    : public extensions::PlatformAppBrowserTest {
 public:
  KeyboardControllerAppWindowTest() {}
  ~KeyboardControllerAppWindowTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerAppWindowTest);
};

// Tests that ime window won't overscroll. See crbug.com/529880.
IN_PROC_BROWSER_TEST_F(KeyboardControllerAppWindowTest,
                       DisableOverscrollForImeWindow) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "test extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Set("background",
                                extensions::DictionaryBuilder()
                                    .Set("scripts", extensions::ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
          .Build();

  extension_service()->AddExtension(extension.get());
  extensions::AppWindow::CreateParams non_ime_params;
  non_ime_params.frame = extensions::AppWindow::FRAME_NONE;
  extensions::AppWindow* non_ime_app_window = CreateAppWindowFromParams(
      browser()->profile(), extension.get(), non_ime_params);
  int non_ime_window_visible_height = non_ime_app_window->web_contents()
                                          ->GetRenderWidgetHostView()
                                          ->GetVisibleViewportSize()
                                          .height();

  extensions::AppWindow::CreateParams ime_params;
  ime_params.frame = extensions::AppWindow::FRAME_NONE;
  ime_params.is_ime_window = true;
  extensions::AppWindow* ime_app_window = CreateAppWindowFromParams(
      browser()->profile(), extension.get(), ime_params);
  int ime_window_visible_height = ime_app_window->web_contents()
                                      ->GetRenderWidgetHostView()
                                      ->GetVisibleViewportSize()
                                      .height();

  ASSERT_EQ(non_ime_window_visible_height, ime_window_visible_height);
  ASSERT_TRUE(ime_window_visible_height > 0);

  // Make sure the keyboard has loaded before showing.
  KeyboardLoadedWaiter().Wait();

  auto* controller = ChromeKeyboardControllerClient::Get();
  controller->ShowKeyboard();
  KeyboardVisibleWaiter(true).Wait();

  int screen_height = display::Screen::GetScreen()
                          ->GetPrimaryDisplay()
                          .GetSizeInPixel()
                          .height();
  int keyboard_height = screen_height - ime_window_visible_height + 1;
  ASSERT_GT(keyboard_height, 0);
  gfx::Rect test_bounds = controller->GetKeyboardWindow()->bounds();
  test_bounds.set_height(keyboard_height);
  {
    // Waiter needs to be created before SetBounds() is invoked so that it can
    // catch OnOccludedBoundsChanged event even before it starts waiting.
    KeyboardOccludedBoundsChangeWaiter waiter;
    controller->GetKeyboardWindow()->SetBounds(test_bounds);
    // Wait for the keyboard bounds change has been processed.
    waiter.Wait();
  }

  // Non ime window should have smaller visible view port due to overlap with
  // virtual keyboard.
  EXPECT_LT(non_ime_app_window->web_contents()
                ->GetRenderWidgetHostView()
                ->GetVisibleViewportSize()
                .height(),
            non_ime_window_visible_height);
  // Ime window should have not be affected by virtual keyboard.
  EXPECT_EQ(ime_app_window->web_contents()
                ->GetRenderWidgetHostView()
                ->GetVisibleViewportSize()
                .height(),
            ime_window_visible_height);
}

class KeyboardControllerStateTest : public InProcessBrowserTest {
 public:
  KeyboardControllerStateTest() {}
  ~KeyboardControllerStateTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerStateTest);
};

IN_PROC_BROWSER_TEST_F(KeyboardControllerStateTest, OpenTwice) {
  auto* controller = ChromeKeyboardControllerClient::Get();

  EXPECT_FALSE(controller->is_keyboard_visible());

  // Call ShowKeyboard twice, the keyboard should become visible.
  controller->ShowKeyboard();
  controller->ShowKeyboard();
  KeyboardVisibleWaiter(true).Wait();
  EXPECT_TRUE(controller->is_keyboard_visible());

  // Ensure the keyboard remains visible. Note: we call RunUntilIdle to at least
  // ensure no other messages are pending instead of relying on a timeout that
  // will slow down tests and potentially be flakey.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_F(KeyboardControllerStateTest, OpenAndCloseAndOpen) {
  auto* controller = ChromeKeyboardControllerClient::Get();
  controller->ShowKeyboard();
  KeyboardVisibleWaiter(true).Wait();

  controller->HideKeyboard(ash::HideReason::kSystem);
  KeyboardVisibleWaiter(false).Wait();

  controller->ShowKeyboard();
  KeyboardVisibleWaiter(true).Wait();
}

// NOTE: The following tests test internal state of keyboard::KeyboardController
// and will not work in Multi Process Mash. TODO(stevenjb/shend): Determine
// whether this needs to be tested in a keyboard::KeyboardController unit test.

IN_PROC_BROWSER_TEST_F(KeyboardControllerStateTest, StateResolvesAfterPreload) {
  auto* controller = keyboard::KeyboardUIController::Get();
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kLoading);
  KeyboardLoadedWaiter().Wait();
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kHidden);
}

IN_PROC_BROWSER_TEST_F(KeyboardControllerStateTest,
                       OpenAndCloseAndOpenInternal) {
  auto* controller = keyboard::KeyboardUIController::Get();
  controller->ShowKeyboard(false);
  // Need to wait the extension to be loaded. Hence LOADING_EXTENSION.
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kLoading);
  KeyboardVisibleWaiter(true).Wait();

  controller->HideKeyboardExplicitlyBySystem();
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kHidden);

  controller->ShowKeyboard(false);
  // The extension already has been loaded. Hence SHOWING.
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kShown);
}

// See crbug.com/755354.
IN_PROC_BROWSER_TEST_F(KeyboardControllerStateTest,
                       DisablingKeyboardGoesToInitialState) {
  auto* controller = keyboard::KeyboardUIController::Get();

  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kLoading);

  controller->Shutdown();
  EXPECT_EQ(controller->GetStateForTest(), keyboard::KeyboardUIState::kInitial);
}
