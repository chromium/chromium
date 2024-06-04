// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_web_contents_factory.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/menu_button.h"

namespace {

// A test delegate for a toolbar action view.
class TestToolbarActionViewDelegate : public ToolbarActionView::Delegate {
 public:
  TestToolbarActionViewDelegate()
      : overflow_reference_view_(std::make_unique<views::MenuButton>()),
        web_contents_(nullptr) {}
  TestToolbarActionViewDelegate(const TestToolbarActionViewDelegate&) = delete;
  TestToolbarActionViewDelegate& operator=(
      const TestToolbarActionViewDelegate&) = delete;
  ~TestToolbarActionViewDelegate() override = default;

  // ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override {
    return web_contents_;
  }
  views::MenuButton* GetOverflowReferenceView() const override {
    return overflow_reference_view_.get();
  }
  gfx::Size GetToolbarActionSize() override { return gfx::Size(32, 32); }
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {}
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override {
    return ui::DragDropTypes::DRAG_NONE;
  }
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override { return false; }

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  std::unique_ptr<views::MenuButton> overflow_reference_view_;

  raw_ptr<content::WebContents> web_contents_;
};

class OpenMenuListener : public views::ContextMenuController {
 public:
  explicit OpenMenuListener(views::View* view)
      : view_(view),
        opened_menu_(false) {
    view_->set_context_menu_controller(this);
  }
  OpenMenuListener(const OpenMenuListener&) = delete;
  OpenMenuListener& operator=(const OpenMenuListener&) = delete;
  ~OpenMenuListener() override {
    view_->set_context_menu_controller(nullptr);
  }

  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    opened_menu_ = true;
  }

  bool opened_menu() const { return opened_menu_; }

 private:
  raw_ptr<views::View> view_;

  bool opened_menu_;
};

}  // namespace

class ToolbarActionViewUnitTest : public ChromeViewsTestBase {
 public:
  ToolbarActionViewUnitTest() : widget_(nullptr) {}
  ToolbarActionViewUnitTest(const ToolbarActionViewUnitTest&) = delete;
  ToolbarActionViewUnitTest& operator=(const ToolbarActionViewUnitTest&) =
      delete;
  ~ToolbarActionViewUnitTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    controller_ =
        std::make_unique<TestToolbarActionViewController>("fake controller");
    action_view_delegate_ = std::make_unique<TestToolbarActionViewDelegate>();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

  TestToolbarActionViewController* controller() { return controller_.get(); }

  TestToolbarActionViewDelegate* action_view_delegate() {
    return action_view_delegate_.get();
  }

 private:
  std::unique_ptr<TestToolbarActionViewController> controller_;
  std::unique_ptr<TestToolbarActionViewDelegate> action_view_delegate_;

  // The widget managed by this test.
  std::unique_ptr<views::Widget> widget_;
};

// A MenuButton subclass that provides access to some MenuButton internals.
class TestToolbarActionView : public ToolbarActionView {
 public:
  TestToolbarActionView(ToolbarActionViewController* view_controller,
                        Delegate* delegate)
      : ToolbarActionView(view_controller, delegate) {}
  TestToolbarActionView(const TestToolbarActionView&) = delete;
  TestToolbarActionView& operator=(const TestToolbarActionView&) = delete;
  ~TestToolbarActionView() override {}
};

// Verifies there is no crash when a ToolbarActionView with an InkDrop is
// destroyed while holding a |pressed_lock_|.
TEST_F(ToolbarActionViewUnitTest,
       NoCrashWhenDestroyingToolbarActionViewThatHasAPressedLock) {
  TestToolbarActionViewController* view_controller = controller();

  // Create a new toolbar action view.
  auto view = std::make_unique<ToolbarActionView>(view_controller,
                                                  action_view_delegate());
  view->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
  widget()->SetContentsView(std::move(view));
  widget()->Show();

  view_controller->ShowPopup(true);
}

// Verifies the InkDropAnimation used by the ToolbarActionView doesn't fail a
// DCHECK for an unsupported transition from ACTIVATED to ACTION_PENDING.
TEST_F(ToolbarActionViewUnitTest,
       NoCrashWhenPressingMouseOnToolbarActionViewThatHasAPressedLock) {
  TestToolbarActionViewController* view_controller = controller();

  // Create a new toolbar action view.
  auto view = std::make_unique<ToolbarActionView>(view_controller,
                                                  action_view_delegate());
  view->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
  widget()->SetContentsView(std::move(view));
  widget()->Show();

  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());

  view_controller->ShowPopup(true);
  generator.PressLeftButton();
}

// Test the basic ui of a ToolbarActionView and that it responds correctly to
// a controller's state.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN)
// TODO(crbug.com/40668368): Test is flaky on Mac, Linux and Win10.
#define MAYBE_BasicToolbarActionViewTest DISABLED_BasicToolbarActionViewTest
#else
#define MAYBE_BasicToolbarActionViewTest BasicToolbarActionViewTest
#endif
TEST_F(ToolbarActionViewUnitTest, MAYBE_BasicToolbarActionViewTest) {
  TestingProfile profile;

  // ViewsTestBase initializes the aura environment, so the factory shouldn't.
  content::TestWebContentsFactory web_contents_factory;

  TestToolbarActionViewController* view_controller = controller();
  TestToolbarActionViewDelegate* view_delegate = action_view_delegate();

  // Configure the test controller and delegate.
  std::u16string name = u"name";
  view_controller->SetAccessibleName(name);
  std::u16string tooltip = u"tooltip";
  view_controller->SetTooltip(tooltip);
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(&profile);
  CreateSessionServiceTabHelper(web_contents);
  view_delegate->set_web_contents(web_contents);

  // Move the mouse off the not-yet-existent button.
  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());
  generator.MoveMouseTo(gfx::Point(300, 300));

  // Create a new toolbar action view.
  auto owning_view =
      std::make_unique<ToolbarActionView>(view_controller, view_delegate);
  owning_view->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
  ToolbarActionView* view = owning_view.get();
  widget()->SetContentsView(std::move(owning_view));
  widget()->Show();

  // Check that the tooltip and accessible state of the view match the
  // controller's.
  EXPECT_EQ(tooltip, view->GetTooltipText(gfx::Point()));
  ui::AXNodeData ax_node_data;
  view->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(name, ax_node_data.GetString16Attribute(
                      ax::mojom::StringAttribute::kName));

  // The button should start in normal state, with no actions executed.
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());
  EXPECT_EQ(0, view_controller->execute_action_count());

  // Click the button. This should execute it.
  generator.MoveMouseTo(gfx::Point(10, 10));
  generator.ClickLeftButton();
  EXPECT_EQ(1, view_controller->execute_action_count());

  // Move the mouse off the button, and show a popup through a non-user action.
  // Since this was not a user action, the button should not be pressed.
  generator.MoveMouseTo(gfx::Point(300, 300));
  view_controller->ShowPopup(false);
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());
  view_controller->HidePopup();

  // Show the popup through a user action - the button should be pressed.
  view_controller->ShowPopup(true);
  EXPECT_EQ(views::Button::STATE_PRESSED, view->GetState());
  view_controller->HidePopup();
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());

  // Ensure that clicking on an otherwise-disabled action opens the
  // context menu.
  view_controller->SetEnabled(false);
  // Even though the controller is disabled, the button remains enabled
  // because it will open the context menu.
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());
  int old_execute_action_count = view_controller->execute_action_count();
  {
    OpenMenuListener menu_listener(view);
    view->Activate(nullptr);
    EXPECT_TRUE(menu_listener.opened_menu());
    EXPECT_EQ(old_execute_action_count,
              view_controller->execute_action_count());
  }

  // Create an overflow button.
  views::MenuButton* overflow_button =
      view_delegate->GetOverflowReferenceView();

  // If the view isn't visible, the overflow button should be pressed for
  // popups.
  view->SetVisible(false);
  view_controller->ShowPopup(true);
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, overflow_button->GetState());
  view_controller->HidePopup();
  EXPECT_EQ(views::Button::STATE_NORMAL, view->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, overflow_button->GetState());
}
