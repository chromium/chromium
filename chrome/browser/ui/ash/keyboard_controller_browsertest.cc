// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_resource_util.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace {
const int kKeyboardHeightForTest = 100;
}  // namespace

class VirtualKeyboardWebContentTest : public InProcessBrowserTest {
 public:
  VirtualKeyboardWebContentTest() {}
  ~VirtualKeyboardWebContentTest() override {}

  void SetUp() override {
    ui::SetUpInputMethodFactoryForTesting();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

  ui::InputMethod* GetInputMethod() {
    return keyboard::KeyboardController::Get()->GetInputMethodForTest();
  }

 protected:
  void FocusEditableNodeAndShowKeyboard(const gfx::Rect& init_bounds) {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
    auto* keyboard_controller = keyboard::KeyboardController::Get();
    ui::InputMethod* input_method =
        keyboard_controller->GetInputMethodForTest();
    input_method->SetFocusedTextInputClient(client.get());
    input_method->ShowVirtualKeyboardIfEnabled();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    keyboard_controller->GetKeyboardWindow()->SetBounds(init_bounds);
    // Mock KeyboardUI notifying KeyboardController that the contents loaded.
    keyboard_controller->NotifyKeyboardWindowLoaded();
  }

  void FocusNonEditableNode() {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_NONE));
    keyboard::KeyboardController::Get()
        ->GetInputMethodForTest()
        ->SetFocusedTextInputClient(client.get());
  }

  void MockEnableIMEInDifferentExtension(const std::string& url,
                                         const gfx::Rect& init_bounds) {
    DCHECK(!url.empty());
    ChromeKeyboardControllerClient::Get()->set_virtual_keyboard_url_for_test(
        GURL(url));
    auto* keyboard_controller = keyboard::KeyboardController::Get();
    keyboard_controller->Reload();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    keyboard_controller->GetKeyboardWindow()->SetBounds(init_bounds);
  }

  bool IsKeyboardVisible() const {
    return keyboard::KeyboardController::Get()->IsKeyboardVisible();
  }

 private:
  std::unique_ptr<ui::DummyTextInputClient> client;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWebContentTest);
};

// Test for crbug.com/404340. After enabling an IME in a different extension,
// its virtual keyboard should not become visible if previous one is not.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       EnableIMEInDifferentExtension) {
  gfx::Rect test_bounds(0, 0, 0, kKeyboardHeightForTest);
  FocusEditableNodeAndShowKeyboard(test_bounds);
  EXPECT_TRUE(IsKeyboardVisible());
  FocusNonEditableNode();
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-1", test_bounds);
  // Keyboard should not become visible if previous keyboard is not.
  EXPECT_FALSE(IsKeyboardVisible());

  FocusEditableNodeAndShowKeyboard(test_bounds);
  // Keyboard should become visible after focus on an editable node.
  EXPECT_TRUE(IsKeyboardVisible());

  // Simulate hide keyboard by pressing hide key on the virtual keyboard.
  keyboard::KeyboardController::Get()->HideKeyboardByUser();
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-2", test_bounds);
  // Keyboard should not become visible if previous keyboard is not, even if it
  // is currently focused on an editable node.
  EXPECT_FALSE(IsKeyboardVisible());
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       CanDragFloatingKeyboardWithMouse) {
  auto* controller = keyboard::KeyboardController::Get();
  controller->SetContainerType(keyboard::ContainerType::FLOATING, base::nullopt,
                               base::DoNothing());

  controller->ShowKeyboard(false);
  WaitControllerStateChangesTo(keyboard::KeyboardControllerState::SHOWN);

  aura::Window* contents_window = controller->GetKeyboardWindow();
  contents_window->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Point(0, 0), contents_window->bounds().origin());

  controller->SetDraggableArea(contents_window->bounds());

  // Drag the top left corner of the keyboard to move it.
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());
  event_generator.MoveMouseTo(gfx::Point(0, 0));
  event_generator.PressLeftButton();
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  event_generator.ReleaseLeftButton();
  event_generator.MoveMouseTo(gfx::Point(100, 100));

  EXPECT_EQ(gfx::Point(50, 50), contents_window->bounds().origin());
}

// A test for crbug.com/734534
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       DoesNotCrashWhenParentDoesNotExist) {
  auto* controller = keyboard::KeyboardController::Get();

  controller->LoadKeyboardWindowInBackground();

  aura::Window* view = controller->GetKeyboardWindow();
  EXPECT_TRUE(view);

  // Remove the keyboard window parent.
  EXPECT_TRUE(view->parent());
  controller->DeactivateKeyboard();
  EXPECT_FALSE(view->parent());

  // Change window size to trigger OnWindowBoundsChanged.
  view->SetBounds(gfx::Rect(0, 0, 1200, 800));
}

class VirtualKeyboardAppWindowTest : public extensions::PlatformAppBrowserTest {
 public:
  VirtualKeyboardAppWindowTest() {}
  ~VirtualKeyboardAppWindowTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardAppWindowTest);
};

// Tests that ime window won't overscroll. See crbug.com/529880.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardAppWindowTest,
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

  int screen_height = ash::Shell::GetPrimaryRootWindow()->bounds().height();
  gfx::Rect test_bounds(0, 0, 0, screen_height - ime_window_visible_height + 1);
  auto* controller = keyboard::KeyboardController::Get();
  controller->ShowKeyboard(false /* locked */);
  controller->NotifyKeyboardWindowLoaded();
  controller->GetKeyboardWindow()->SetBounds(test_bounds);

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

class VirtualKeyboardStateTest : public InProcessBrowserTest {
 public:
  VirtualKeyboardStateTest() {}
  ~VirtualKeyboardStateTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardStateTest);
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardStateTest, OpenTwice) {
  auto* controller = keyboard::KeyboardController::Get();

  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);
  // Call ShowKeyboard twice. The second call should has no effect.
  controller->ShowKeyboard(false);
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);
  controller->ShowKeyboard(false);
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);

  WaitControllerStateChangesTo(keyboard::KeyboardControllerState::SHOWN);
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::SHOWN);
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardStateTest, StateResolvesAfterPreload) {
  auto* controller = keyboard::KeyboardController::Get();

  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);
  WaitControllerStateChangesTo(keyboard::KeyboardControllerState::HIDDEN);
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardStateTest, OpenAndCloseAndOpen) {
  auto* controller = keyboard::KeyboardController::Get();

  controller->ShowKeyboard(false);
  // Need to wait the extension to be loaded. Hence LOADING_EXTENSION.
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);
  WaitControllerStateChangesTo(keyboard::KeyboardControllerState::SHOWN);

  controller->HideKeyboardExplicitlyBySystem();
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::HIDDEN);

  controller->ShowKeyboard(false);
  // The extension already has been loaded. Hence SHOWING.
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::SHOWN);
}

// See crbug.com/755354.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardStateTest,
                       DisablingKeyboardGoesToInitialState) {
  auto* controller = keyboard::KeyboardController::Get();

  controller->LoadKeyboardWindowInBackground();
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::LOADING_EXTENSION);

  controller->DisableKeyboard();
  EXPECT_EQ(controller->GetStateForTest(),
            keyboard::KeyboardControllerState::INITIAL);
}
