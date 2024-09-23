// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace test {

// Friend of ToolbarButton to access private members.
class ToolbarButtonTestApi {
 public:
  explicit ToolbarButtonTestApi(ToolbarButton* button) : button_(button) {}
  ToolbarButtonTestApi(const ToolbarButtonTestApi&) = delete;
  ToolbarButtonTestApi& operator=(const ToolbarButtonTestApi&) = delete;

  views::MenuRunner* menu_runner() { return button_->menu_runner_.get(); }
  bool menu_showing() const { return button_->menu_showing_; }

  const gfx::Insets last_paint_insets() const {
    return button_->last_paint_insets_;
  }
  const gfx::Insets layout_inset_delta() const {
    return button_->layout_inset_delta_;
  }
  const std::optional<SkColor> last_border_color() const {
    return button_->last_border_color_;
  }
  void SetAnimationTimingForTesting() {
    button_->highlight_color_animation_.highlight_color_animation_
        .SetSlideDuration(base::TimeDelta());
  }

 private:
  raw_ptr<ToolbarButton> button_;
};

}  // namespace test

namespace {

class CheckActiveWebContentsMenuModel : public ui::SimpleMenuModel {
 public:
  explicit CheckActiveWebContentsMenuModel(TabStripModel* tab_strip_model)
      : SimpleMenuModel(nullptr), tab_strip_model_(tab_strip_model) {
    DCHECK(tab_strip_model_);
  }
  CheckActiveWebContentsMenuModel(const CheckActiveWebContentsMenuModel&) =
      delete;
  CheckActiveWebContentsMenuModel& operator=(
      const CheckActiveWebContentsMenuModel&) = delete;
  ~CheckActiveWebContentsMenuModel() override = default;

  // ui::SimpleMenuModel:
  size_t GetItemCount() const override {
    EXPECT_TRUE(tab_strip_model_->GetActiveWebContents());
    return 0;
  }

 private:
  const raw_ptr<TabStripModel> tab_strip_model_;
};

class TestToolbarButton : public ToolbarButton {
 public:
  using ToolbarButton::ToolbarButton;

  void ResetBorderUpdateFlag() { did_border_update_ = false; }
  bool did_border_update() const { return did_border_update_; }

  // ToolbarButton:
  void SetBorder(std::unique_ptr<views::Border> b) override {
    ToolbarButton::SetBorder(std::move(b));
    did_border_update_ = true;
  }

 private:
  bool did_border_update_ = false;
};

}  // namespace

using ToolbarButtonViewsTest = ChromeViewsTestBase;

TEST_F(ToolbarButtonViewsTest, NoDefaultLayoutInsets) {
  ToolbarButton button;
  gfx::Insets default_insets = ::GetLayoutInsets(TOOLBAR_BUTTON);
  // Colors and insets are not ready until OnThemeChanged()
  button.OnThemeChanged();
  EXPECT_FALSE(button.GetLayoutInsets().has_value());
  EXPECT_EQ(default_insets, button.GetInsets());
}

TEST_F(ToolbarButtonViewsTest, SetLayoutInsets) {
  ToolbarButton button;
  auto new_insets = gfx::Insets::TLBR(2, 3, 4, 5);
  button.SetLayoutInsets(new_insets);
  EXPECT_EQ(new_insets, button.GetLayoutInsets());
  EXPECT_EQ(new_insets, button.GetInsets());
}

TEST_F(ToolbarButtonViewsTest, MenuDoesNotShowWhenTabStripIsEmpty) {
  TestTabStripModelDelegate delegate;
  TestingProfile profile;
  TabStripModel tab_strip(&delegate, &profile);
  EXPECT_FALSE(tab_strip.GetActiveWebContents());
  auto model = std::make_unique<CheckActiveWebContentsMenuModel>(&tab_strip);

  // ToolbarButton needs a parent view on some platforms.
  auto parent_view = std::make_unique<views::View>();
  ToolbarButton* button =
      parent_view->AddChildView(std::make_unique<ToolbarButton>(
          views::Button::PressedCallback(), std::move(model), &tab_strip));
  std::unique_ptr<views::Widget> widget_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget_->SetContentsView(std::move(parent_view));

  // Since |tab_strip| is empty, calling this does not do anything. This is the
  // expected result. If it actually tries to show the menu, then
  // CheckActiveWebContentsMenuModel::GetItemCount() will get called and the
  // EXPECT_TRUE() call inside will fail.
  button->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
}

class ToolbarButtonUITest : public ChromeViewsTestBase {
 public:
  ToolbarButtonUITest() = default;
  ToolbarButtonUITest(const ToolbarButtonUITest&) = delete;
  ToolbarButtonUITest& operator=(const ToolbarButtonUITest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Usually a BackForwardMenuModel is used, but that needs a Browser*. Make
    // something simple with at least one item so a menu gets shown. Note that
    // ToolbarButton takes ownership of the |model|.
    auto model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model->AddItem(0, std::u16string());

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<TestToolbarButton>(
        views::Button::PressedCallback(), std::move(model), nullptr));
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

 protected:
  raw_ptr<TestToolbarButton, DanglingUntriaged> button_ = nullptr;

 private:
  std::unique_ptr<views::Widget> widget_;
};

// Test showing and dismissing a menu to verify menu delegate lifetime.
TEST_F(ToolbarButtonUITest, ShowMenu) {
  test::ToolbarButtonTestApi test_api(button_);

  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->GetState());

  // Show the menu. Note that it is asynchronous.
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(test_api.menu_showing());
  EXPECT_TRUE(test_api.menu_runner());
  EXPECT_TRUE(test_api.menu_runner()->IsRunning());

  // Button should appear pressed when the menu is showing.
  EXPECT_EQ(views::Button::STATE_PRESSED, button_->GetState());

  test_api.menu_runner()->Cancel();

  // Ensure the ToolbarButton's |menu_runner_| member is reset to null.
  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->GetState());
}

// Widget activation on Mac in unit tests is not reliable, so this will also be
// covered by e.g. `ToolbarViewTest.BackButtonMenu`.
#if !BUILDFLAG(IS_MAC)
TEST_F(ToolbarButtonUITest, ShowMenuWithIdentifier) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMenuId);

  test::ToolbarButtonTestApi test_api(button_);
  button_->set_menu_identifier(kMenuId);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kMenuId, views::ElementTrackerViews::GetContextForView(button_),
          base::BindLambdaForTesting(
              [&run_loop](ui::TrackedElement*) { run_loop.Quit(); }));

  button_->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  run_loop.Run();
  test_api.menu_runner()->Cancel();
}
#endif  // !BUILDFLAG(IS_MAC)

// Test deleting a ToolbarButton while its menu is showing.
TEST_F(ToolbarButtonUITest, DeleteWithMenu) {
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_TRUE(test::ToolbarButtonTestApi(button_).menu_runner());
  widget()->SetContentsView(
      std::make_unique<views::View>());  // Deletes |button_|.
}

// Tests to make sure the button's border color is updated as its animation
// color changes.
TEST_F(ToolbarButtonUITest, TestBorderUpdateColorChange) {
  test::ToolbarButtonTestApi test_api(button_);
  test_api.SetAnimationTimingForTesting();

  button_->ResetBorderUpdateFlag();
  for (SkColor border_color : {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE}) {
    EXPECT_FALSE(button_->did_border_update());
    button_->SetHighlight(std::u16string(), border_color);
    EXPECT_EQ(button_->GetBorder()->color(), border_color);
    EXPECT_TRUE(button_->did_border_update());
    button_->ResetBorderUpdateFlag();
  }
}

// Ensures ToolbarButton updates its border on touch mode changes to
// match layout constants.
//
// Regression test for crbug.com/1163451: ToolbarButton updates its
// border on bounds change, which usually happens during layout.
// Updating the border itself invalidates layout if the border change
// results in a new preferred size. But View::SetBoundsRect() sets
// needs_layout_ = false right after the OnBoundsChanged() call.
//
// On touch mode changes the border change only happened after several
// layouts. When the bug occurred, the border was eventually set
// correctly but too late: its final size did not reflect the preferred
// size after the border update.
//
// This test ensures ToolbarButtons update their border promptly after
// the touch mode change, just after the icon update.
TEST_F(ToolbarButtonUITest, BorderUpdatedOnTouchModeSwitch) {
  ui::TouchUiController::TouchUiScoperForTesting touch_mode_override(false);
  EXPECT_EQ(button_->GetInsets(), GetLayoutInsets(TOOLBAR_BUTTON));

  // This constant is different in touch mode.
  touch_mode_override.UpdateState(true);
  EXPECT_EQ(button_->GetInsets(), GetLayoutInsets(TOOLBAR_BUTTON));
}

using ToolbarButtonActionViewInterfaceTest = ToolbarButtonViewsTest;

TEST_F(ToolbarButtonActionViewInterfaceTest, TestActionChanged) {
  auto toolbar_button = std::make_unique<ToolbarButton>();
  EXPECT_FALSE(toolbar_button->GetVectorIconsHasValueForTesting());
  std::unique_ptr<actions::ActionItem> action_item =
      actions::ActionItem::Builder()
          .SetActionId(0)
          .SetEnabled(false)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kErrorIcon))
          .Build();
  toolbar_button->GetActionViewInterface()->ActionItemChangedImpl(
      action_item.get());
  // Test some properties to ensure that the right ActionViewInterface is linked
  // to the view.
  EXPECT_TRUE(toolbar_button->GetVectorIconsHasValueForTesting());
  EXPECT_FALSE(toolbar_button->GetEnabled());
}
