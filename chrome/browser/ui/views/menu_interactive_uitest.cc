// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/views/controls/menu/menu_controller.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/accessibility/platform/ax_platform_node.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace views {
namespace test {

namespace {

class TestButton : public Button {
  METADATA_HEADER(TestButton, Button)

 public:
  TestButton() : Button(Button::PressedCallback()) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

BEGIN_METADATA(TestButton)
END_METADATA

}  // namespace

class MenuControllerUITest : public InProcessBrowserTest {
 public:
  MenuControllerUITest() {}

  MenuControllerUITest(const MenuControllerUITest&) = delete;
  MenuControllerUITest& operator=(const MenuControllerUITest&) = delete;

  // This method creates a MenuRunner, MenuItemView, etc, adds two menu
  // items, shows the menu so that it can calculate the position of the first
  // menu item and move the mouse there, and closes the menu.
  void SetupMenu(Widget* widget) {
    menu_delegate_ = std::make_unique<MenuDelegate>();
    auto menu_item_owning =
        std::make_unique<MenuItemView>(menu_delegate_.get());
    MenuItemView* menu_item = menu_item_owning.get();
    menu_runner_ = std::make_unique<MenuRunner>(
        std::move(menu_item_owning), views::MenuRunner::CONTEXT_MENU);
    first_item_ = menu_item->AppendMenuItem(1, u"One");
    menu_item->AppendMenuItem(2, u"Two");
    // Run the menu, so that the menu item size will be calculated.
    menu_runner_->RunMenuAt(widget, nullptr, gfx::Rect(),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_NONE);
    RunPendingMessages();
    // Figure out the middle of the first menu item.
    mouse_pos_.set_x(first_item_->width() / 2);
    mouse_pos_.set_y(first_item_->height() / 2);
    View::ConvertPointToScreen(first_item_.get(), &mouse_pos_);
    // Move the mouse so that it's where the menu will be shown.
    base::RunLoop run_loop;
    ui_controls::SendMouseMoveNotifyWhenDone(mouse_pos_.x(), mouse_pos_.y(),
                                             run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(first_item_->IsSelected());
    ui::AXNodeData item_node_data;
    first_item_->GetViewAccessibility().GetAccessibleNodeData(&item_node_data);
    EXPECT_EQ(item_node_data.role, ax::mojom::Role::kMenuItem);

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // ChromeOS does not use popup focus override.
    EXPECT_TRUE(first_item_->GetViewAccessibility().IsFocusedForTesting());
#endif
    ui::AXNodeData menu_node_data;
    menu_item->GetSubmenu()->GetViewAccessibility().GetAccessibleNodeData(
        &menu_node_data);
    EXPECT_EQ(menu_node_data.role, ax::mojom::Role::kMenu);
    menu_runner_->Cancel();
    RunPendingMessages();
  }

  void RunPendingMessages() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    first_item_ = nullptr;
    menu_runner_.reset();
    menu_delegate_.reset();
  }

 protected:
  raw_ptr<MenuItemView> first_item_ = nullptr;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<MenuDelegate> menu_delegate_;
  // Middle of first menu item.
  gfx::Point mouse_pos_;
};

// TODO(crbug.com/40765889): Re-enable this test
IN_PROC_BROWSER_TEST_F(MenuControllerUITest, DISABLED_TestMouseOverShownMenu) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  content::ScopedAccessibilityModeOverride ax_mode_override(
      ui::kAXModeComplete);
#endif
  // Create a parent widget.
  auto widget = std::make_unique<views::Widget>();
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
  params.native_widget = CreateNativeWidget(
      NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA, &params, widget.get());
#endif
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();
  views::test::WaitForWidgetActive(widget.get(), true);

  // Create a focused test button, used to assert that it has accessibility
  // focus before and after menu item is active, but not during.
  TestButton button;
  widget->GetContentsView()->AddChildView(&button);
  FocusManager* focus_manager = widget->GetFocusManager();
  focus_manager->SetFocusedView(&button);
  EXPECT_TRUE(button.HasFocus());
  EXPECT_TRUE(button.GetViewAccessibility().IsFocusedForTesting());

  // SetupMenu leaves the mouse position where the first menu item will be
  // when we run the menu.
  AXEventCounter ax_counter(views::AXEventManager::Get());
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 0);
  SetupMenu(widget.get());

  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 1);
  // SetupMenu creates, opens and closes a popup menu, so there will be a
  // a menu popup end. There is also a menu end since it's the last menu.
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 1);
  EXPECT_FALSE(first_item_->IsSelected());
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // ChromeOS does not use popup focus override.
  EXPECT_FALSE(first_item_->GetViewAccessibility().IsFocusedForTesting());
#endif
  menu_runner_->RunMenuAt(widget.get(), nullptr, gfx::Rect(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_NONE);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 2);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 2);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 1);
  EXPECT_FALSE(first_item_->IsSelected());
  // One or two mouse events are posted by the menu being shown.
  // Process event(s), and check what's selected in the menu.
  RunPendingMessages();
  EXPECT_FALSE(first_item_->IsSelected());
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // ChromeOS does not use popup focus override.
  EXPECT_FALSE(first_item_->GetViewAccessibility().IsFocusedForTesting());
  EXPECT_TRUE(button.GetViewAccessibility().IsFocusedForTesting());
#endif
  // Move mouse one pixel to left and verify that the first menu item
  // is selected.
  mouse_pos_.Offset(-1, 0);
  base::RunLoop run_loop2;
  ui_controls::SendMouseMoveNotifyWhenDone(mouse_pos_.x(), mouse_pos_.y(),
                                           run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(first_item_->IsSelected());
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // ChromeOS does not use popup focus override.
  EXPECT_TRUE(first_item_->GetViewAccessibility().IsFocusedForTesting());
  EXPECT_FALSE(button.GetViewAccessibility().IsFocusedForTesting());
#endif
  menu_runner_->Cancel();
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // ChromeOS does not use popup focus override.
  EXPECT_FALSE(first_item_->GetViewAccessibility().IsFocusedForTesting());
  EXPECT_TRUE(button.GetViewAccessibility().IsFocusedForTesting());
#endif
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 2);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 2);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 2);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 2);
  widget->Close();
}

// This test creates a menu without a parent widget, and tests that it
// can receive keyboard events.
// TODO(davidbienvenu): If possible, get test working for linux and
// mac. Only status_icon_win runs a menu with a null parent widget
// currently.
#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(MenuControllerUITest, FocusOnOrphanMenu) {
  // This test is extremely flaky on WIN10_20H2, so disable.
  // TODO(crbug.com/40188371) Investigate why it's so flaky on that version of
  // Windows.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2) {
    GTEST_SKIP() << "Skipping test for WIN10_20H2 and greater";
  }
  // Going into full screen mode prevents pre-test focus and mouse position
  // state from affecting test, and helps ui_controls function correctly.
  chrome::ToggleFullscreenMode(browser());
  content::ScopedAccessibilityModeOverride ax_mode_override(
      ui::kAXModeComplete);
  MenuDelegate menu_delegate;
  auto menu_item_owning = std::make_unique<MenuItemView>(&menu_delegate);
  MenuItemView* menu_item = menu_item_owning.get();
  AXEventCounter ax_counter(views::AXEventManager::Get());
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 0);
  std::unique_ptr<MenuRunner> menu_runner(std::make_unique<MenuRunner>(
      std::move(menu_item_owning), views::MenuRunner::CONTEXT_MENU));
  MenuItemView* first_item = menu_item->AppendMenuItem(1, u"One");
  menu_item->AppendMenuItem(2, u"Two");
  menu_runner->RunMenuAt(nullptr, nullptr, gfx::Rect(),
                         views::MenuAnchorPosition::kTopLeft,
                         ui::MENU_SOURCE_NONE);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 0);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 0);
  base::RunLoop loop;
  // SendKeyPress fails if the window doesn't have focus.
  ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
      menu_item->GetSubmenu()->GetWidget()->GetNativeWindow(), ui::VKEY_DOWN,
      false, false, false, false, loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(first_item->IsSelected());
  EXPECT_TRUE(first_item->GetViewAccessibility().IsFocusedForTesting());
  menu_runner->Cancel();
  EXPECT_FALSE(first_item->GetViewAccessibility().IsFocusedForTesting());
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuStart), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd), 1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kMenuEnd), 1);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace test
}  // namespace views
