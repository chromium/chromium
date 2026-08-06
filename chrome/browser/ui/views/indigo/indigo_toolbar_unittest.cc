// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace indigo {

namespace {

// AnimatingLayoutManager requires a small buffer of time after the main
// animation duration completes to fully resolve its `IsDrawn` UI state
// in a task environment.
constexpr base::TimeDelta kAnimationSettleDuration = base::Milliseconds(100);

class MockIndigoToolbarDelegate : public IndigoToolbar::Delegate {
 public:
  MOCK_METHOD(void, OnClose, (IndigoToolbar*), (override));
  MOCK_METHOD(void, OnRegenerate, (IndigoToolbar*), (override));
  MOCK_METHOD(void, OnReplaceOriginalPhoto, (IndigoToolbar*), (override));
  MOCK_METHOD(void, OnDeleteOriginalPhoto, (IndigoToolbar*), (override));
};

}  // namespace

class IndigoToolbarTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    widget_->Init(std::move(params));
    widget_->SetBounds(gfx::Rect(0, 0, 800, 600));

    overlay_view_ =
        widget_->GetContentsView()->AddChildView(CreateIndigoOverlayView());
    overlay_view_->SetBoundsRect(gfx::Rect(0, 0, 800, 600));

    widget_->Show();

    // Move the mouse away to prevent the system mouse position from
    // inadvertently hovering over the toolbar during testing.
    ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                             widget()->GetNativeWindow());
    event_generator.MoveMouseTo(gfx::Point(1000, 1000));
  }

  void TearDown() override {
    overlay_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }
  views::View* overlay_view() { return overlay_view_; }

  views::View* GetToolbarView() {
    return overlay_view_->GetViewByElementId(IndigoToolbar::kToolbarElementId);
  }

  views::Button* GetButtonFromToolbar(views::View* toolbar_view,
                                      ui::ElementIdentifier id) {
    return views::Button::AsButton(toolbar_view->GetViewByElementId(id));
  }

  views::View* GetOverlayEventHandlerAtCenter(views::View* view) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(view, overlay_view(), &center);
    return overlay_view()->GetEventHandlerForPoint(center);
  }

  void ExpectToolbarContains(views::View* toolbar_view, views::View* child) {
    EXPECT_TRUE(toolbar_view->GetLocalBounds().Contains(
        views::View::ConvertRectToTarget(child, toolbar_view,
                                         child->GetLocalBounds())));
  }

  void ExpandAndWait() {
    views::View* toolbar_view = GetToolbarView();
    views::Button* expand_button = GetButtonFromToolbar(
        toolbar_view, IndigoToolbar::kExpandButtonElementId);
    views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
    auto* animating_container = toolbar_view->GetViewByElementId(
        IndigoToolbar::kAnimatingContainerElementId);
    overlay_view()->DeprecatedLayoutImmediately();
    static_cast<views::AnimatingLayoutManager*>(
        animating_container->GetLayoutManager())
        ->ResetLayout();
    views::Button* regenerate_button = GetButtonFromToolbar(
        toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
    EXPECT_TRUE(regenerate_button->IsDrawn());
  }

  void WaitForCollapse() {
    views::View* toolbar_view = GetToolbarView();
    widget()->GetFocusManager()->ClearFocus();
    auto* animating_container = toolbar_view->GetViewByElementId(
        IndigoToolbar::kAnimatingContainerElementId);
    overlay_view()->DeprecatedLayoutImmediately();
    static_cast<views::AnimatingLayoutManager*>(
        animating_container->GetLayoutManager())
        ->ResetLayout();
    views::Button* regenerate_button = GetButtonFromToolbar(
        toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
    EXPECT_FALSE(regenerate_button->IsDrawn());
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> overlay_view_;
};

TEST_F(IndigoToolbarTest, CloseAndReopen) {
  base::UserActionTester user_action_tester;
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  EXPECT_CALL(delegate, OnClose(toolbar.get())).Times(1);

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  // The close button removes the toolbar view from its parent.
  auto* close_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kCloseButtonElementId);
  ASSERT_NE(close_button, nullptr);
  views::test::ButtonTestApi(close_button).NotifyDefaultMouseClick();
  EXPECT_EQ(GetToolbarView(), nullptr);

  // Show again
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  views::View* toolbar_view_after_close = GetToolbarView();
  ASSERT_NE(toolbar_view_after_close, nullptr);
  EXPECT_TRUE(toolbar_view_after_close->GetVisible());

  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Toolbar.Close"), 1);
}

// TODO(crbug.com/536086195): Flaky on all platforms.
TEST_F(IndigoToolbarTest, DISABLED_ExpandCollapseInteractions) {
  base::UserActionTester user_action_tester;
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_CALL(delegate, OnClose(toolbar.get())).Times(0);

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  auto* expand_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kExpandButtonElementId);
  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
  auto* replace_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kReplacePhotoButtonElementId);
  auto* delete_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kDeletePhotoButtonElementId);
  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);

  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());

  // Expand the toolbar.
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Toolbar.Expand"), 1);
  task_environment()->FastForwardBy(kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(regenerate_button->IsDrawn());
  EXPECT_TRUE(replace_photo_button->IsDrawn());
  EXPECT_TRUE(delete_photo_button->IsDrawn());

  // A manually expanded toolbar does not auto-compact.
  task_environment()->FastForwardBy(kInitialAutoCompactDelay);
  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_TRUE(regenerate_button->IsDrawn());

  // Interact with expanded buttons.
  EXPECT_CALL(delegate, OnRegenerate(toolbar.get())).Times(1);
  views::test::ButtonTestApi(regenerate_button).NotifyDefaultMouseClick();
  EXPECT_CALL(delegate, OnReplaceOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(replace_photo_button).NotifyDefaultMouseClick();
  EXPECT_CALL(delegate, OnDeleteOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(delete_photo_button).NotifyDefaultMouseClick();

  // Move the mouse over the toolbar to trigger `is_interacting_ = true`.
  // This explicitly locks out the `AutoCompactTimer` from starting when we
  // close the expanded layout. This bypasses a race condition in
  // `AnimatingLayoutManager` where calling FadeOut on a view currently
  // finishing its previous FadeOut prevents it from properly updating its
  // visibility.
  // Move the OS-level cursor to prevent Windows from synthesizing
  // OnMouseExited.
  ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                           widget()->GetNativeWindow());
  event_generator.MoveMouseTo(expand_button->GetBoundsInScreen().CenterPoint());
  // Manually and synchronously dispatch the enter event to bypass flaky
  // asynchronous event delivery across POSIX test environments.
  ui::MouseEvent mouse_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), 0, 0);
  toolbar_view->OnMouseEntered(mouse_event);

  // Collapse the toolbar.
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Toolbar.Expand"), 2);
  task_environment()->FastForwardBy(kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());
}

TEST_F(IndigoToolbarTest, AutoCompactsAndHoverExpands) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  auto* expand_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kExpandButtonElementId);
  ASSERT_NE(expand_button, nullptr);

  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);
  ASSERT_NE(spark_icon, nullptr);

  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
  ASSERT_NE(regenerate_button, nullptr);

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
  const int collapsed_width = toolbar_view->width();

  ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                           widget()->GetNativeWindow());
  event_generator.MoveMouseTo(gfx::Point(1000, 1000));

  task_environment()->FastForwardBy(kInitialAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_LT(toolbar_view->width(), collapsed_width);

  event_generator.MoveMouseTo(expand_button->GetBoundsInScreen().CenterPoint());

  task_environment()->FastForwardBy(kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());

  event_generator.MoveMouseTo(gfx::Point(1000, 1000));
  task_environment()->FastForwardBy(kInteractionAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
}

TEST_F(IndigoToolbarTest, CompactToolbarExpandsOnKeyboardFocus) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* expand_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kExpandButtonElementId);
  ASSERT_NE(expand_button, nullptr);
  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);
  ASSERT_NE(spark_icon, nullptr);
  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
  ASSERT_NE(regenerate_button, nullptr);

  task_environment()->FastForwardBy(kInitialAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());

  views::test::WaitForWidgetActive(widget(), true);
  widget()->GetFocusManager()->SetKeyboardAccessible(true);
  expand_button->RequestFocus();
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(expand_button->HasFocus());
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());

  widget()->GetFocusManager()->ClearFocus();
  task_environment()->FastForwardBy(kInteractionAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
}

TEST_F(IndigoToolbarTest, HoverCloseButtonInCompactDoesNotExpand) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* close_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kCloseButtonElementId);
  ASSERT_NE(close_button, nullptr);
  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);
  ASSERT_NE(spark_icon, nullptr);

  task_environment()->FastForwardBy(kInitialAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(spark_icon->IsDrawn());

  ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                           widget()->GetNativeWindow());
  event_generator.MoveMouseTo(close_button->GetBoundsInScreen().CenterPoint());

  // Manually and synchronously dispatch the enter event to bypass flaky
  // asynchronous event delivery across POSIX test environments.
  ui::MouseEvent mouse_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), 0, 0);
  close_button->OnMouseEntered(mouse_event);
  toolbar_view->OnMouseEntered(mouse_event);

  // Forward time to allow any erroneous timers/animations to trigger
  task_environment()->FastForwardBy(kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  // It should STILL be in compact mode because hover was over close button
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_TRUE(close_button->IsDrawn());
}

TEST_F(IndigoToolbarTest, HoverExpandThenHoverCloseKeepsExpanded) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* expand_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kExpandButtonElementId);
  ASSERT_NE(expand_button, nullptr);
  auto* close_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kCloseButtonElementId);
  ASSERT_NE(close_button, nullptr);
  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);
  ASSERT_NE(spark_icon, nullptr);
  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
  ASSERT_NE(regenerate_button, nullptr);

  ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                           widget()->GetNativeWindow());
  event_generator.MoveMouseTo(gfx::Point(1000, 1000));

  // Wait for initial auto compact
  task_environment()->FastForwardBy(kInitialAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(spark_icon->IsDrawn());

  // Hover over expand button
  event_generator.MoveMouseTo(expand_button->GetBoundsInScreen().CenterPoint());

  // Manually and synchronously dispatch the enter event to bypass flaky
  // asynchronous event delivery across POSIX test environments.
  ui::MouseEvent mouse_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), 0, 0);
  expand_button->OnMouseEntered(mouse_event);
  toolbar_view->OnMouseEntered(mouse_event);

  // Forward time for expand animation
  task_environment()->FastForwardBy(kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  // It should be expanded (uncompacted)
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());

  // Hover over close button
  event_generator.MoveMouseTo(close_button->GetBoundsInScreen().CenterPoint());

  // Synchronously update hover state
  ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                            gfx::Point(), base::TimeTicks::Now(), 0, 0);
  expand_button->OnMouseExited(exit_event);
  close_button->OnMouseEntered(mouse_event);

  // Forward time to allow any erroneous timers/animations to trigger
  task_environment()->FastForwardBy(kInteractionAutoCompactDelay +
                                    kToolbarAnimationDuration +
                                    kAnimationSettleDuration);
  overlay_view()->DeprecatedLayoutImmediately();

  // It should STILL be uncompacted because we moved to another element in the
  // toolbar
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
}

TEST_F(IndigoToolbarTest, ToolbarBounds) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  // Show with empty rect (production initial state)
  toolbar->Show(overlay_view());
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  // Should be invisible initially
  EXPECT_FALSE(toolbar_view->GetVisible());

  // Now simulate compositor update with real bounds
  toolbar->UpdateTrackedPosition(gfx::Rect(100, 100, 400, 300));
  overlay_view()->DeprecatedLayoutImmediately();

  // Should now be visible and moved away from fallback position
  EXPECT_TRUE(toolbar_view->GetVisible());
  EXPECT_NE(toolbar_view->bounds().origin(), gfx::Point(20, 20));
  // Size should still be correct
  EXPECT_LT(toolbar_view->bounds().height(), 100);
  EXPECT_LT(toolbar_view->bounds().width(), 350);

  // Simulate compositor update with empty bounds (image out of view)
  toolbar->UpdateTrackedPosition(gfx::Rect());
  overlay_view()->DeprecatedLayoutImmediately();

  // Should be hidden again
  EXPECT_FALSE(toolbar_view->GetVisible());
}

TEST_F(IndigoToolbarTest, Accessibility) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  auto verify_button_a11y = [&](ui::ElementIdentifier id,
                                int name_id) -> views::Button* {
    views::Button* button = GetButtonFromToolbar(toolbar_view, id);
    EXPECT_NE(button, nullptr) << "Missing button: " << id.GetName();
    if (!button) {
      return nullptr;
    }
    ui::AXNodeData data;
    button->GetViewAccessibility().GetAccessibleNodeData(&data);
    EXPECT_EQ(data.role, ax::mojom::Role::kButton);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringUTF16(name_id));
    return button;
  };

  ui::AXNodeData toolbar_data;
  toolbar_view->GetViewAccessibility().GetAccessibleNodeData(&toolbar_data);
  EXPECT_EQ(toolbar_data.role, ax::mojom::Role::kToolbar);
  EXPECT_EQ(
      toolbar_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_CAPTION));

  views::Button* expand_button = verify_button_a11y(
      IndigoToolbar::kExpandButtonElementId, IDS_INDIGO_TOOLBAR_EXPAND);
  ASSERT_NE(expand_button, nullptr);
  verify_button_a11y(IndigoToolbar::kCloseButtonElementId,
                     IDS_INDIGO_TOOLBAR_CLOSE_ACCESSIBLE_NAME);

  ui::AXNodeData collapsed_data;
  expand_button->GetViewAccessibility().GetAccessibleNodeData(&collapsed_data);
  EXPECT_TRUE(collapsed_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(collapsed_data.HasState(ax::mojom::State::kExpanded));

  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  verify_button_a11y(IndigoToolbar::kExpandButtonElementId,
                     IDS_INDIGO_TOOLBAR_COLLAPSE);

  ui::AXNodeData expanded_data;
  expand_button->GetViewAccessibility().GetAccessibleNodeData(&expanded_data);
  EXPECT_TRUE(expanded_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(expanded_data.HasState(ax::mojom::State::kCollapsed));

  verify_button_a11y(IndigoToolbar::kRegenerateButtonElementId,
                     IDS_INDIGO_TOOLBAR_REGENERATE);
  verify_button_a11y(IndigoToolbar::kReplacePhotoButtonElementId,
                     IDS_INDIGO_TOOLBAR_REPLACE_ORIGINAL_PHOTO);
  verify_button_a11y(IndigoToolbar::kDeletePhotoButtonElementId,
                     IDS_INDIGO_TOOLBAR_DELETE_ORIGINAL_PHOTO);

  // Collapse before hiding to prevent LSan compositor layer leaks.
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  toolbar->Hide();
}

TEST_F(IndigoToolbarTest, OptionsAutoClose_Regenerate) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* const regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);

  // Test Regenerate
  ExpandAndWait();
  EXPECT_CALL(delegate, OnRegenerate(toolbar.get())).Times(1);
  views::test::ButtonTestApi(regenerate_button).NotifyDefaultMouseClick();
  WaitForCollapse();
}

TEST_F(IndigoToolbarTest, OptionsAutoClose_ReplacePhoto) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* const replace_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kReplacePhotoButtonElementId);

  // Test Replace Photo
  ExpandAndWait();
  EXPECT_CALL(delegate, OnReplaceOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(replace_photo_button).NotifyDefaultMouseClick();
  WaitForCollapse();
}

TEST_F(IndigoToolbarTest, OptionsAutoClose_DeletePhoto) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  overlay_view()->DeprecatedLayoutImmediately();

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);
  auto* const delete_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kDeletePhotoButtonElementId);

  // Test Delete Photo
  ExpandAndWait();
  EXPECT_CALL(delegate, OnDeleteOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(delete_photo_button).NotifyDefaultMouseClick();
  WaitForCollapse();
}

}  // namespace indigo
