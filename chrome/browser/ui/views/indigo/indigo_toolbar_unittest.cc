// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>

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
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace indigo {

namespace {

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

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> overlay_view_;
};

TEST_F(IndigoToolbarTest, CloseAndReopen) {
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
}

TEST_F(IndigoToolbarTest, ExpandCollapseInteractions) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(overlay_view());
  toolbar->UpdateTrackedPosition(gfx::Rect(10, 10, 100, 100));
  EXPECT_CALL(delegate, OnClose(toolbar.get())).Times(0);

  views::View* toolbar_view = GetToolbarView();
  ASSERT_NE(toolbar_view, nullptr);

  auto* expand_button =
      GetButtonFromToolbar(toolbar_view, IndigoToolbar::kExpandButtonElementId);
  ASSERT_NE(expand_button, nullptr);

  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kRegenerateButtonElementId);
  ASSERT_NE(regenerate_button, nullptr);

  auto* replace_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kReplacePhotoButtonElementId);
  ASSERT_NE(replace_photo_button, nullptr);

  auto* delete_photo_button = GetButtonFromToolbar(
      toolbar_view, IndigoToolbar::kDeletePhotoButtonElementId);
  ASSERT_NE(delete_photo_button, nullptr);

  views::View* spark_icon =
      toolbar_view->GetViewByElementId(IndigoToolbar::kSparkIconElementId);
  ASSERT_NE(spark_icon, nullptr);

  // Buttons are initially not drawn.
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());

  std::u16string tooltip_collapsed =
      expand_button->GetRenderedTooltipText(gfx::Point());
  EXPECT_EQ(tooltip_collapsed,
            l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_EXPAND));

  // Expand the toolbar.
  gfx::Point initial_origin = toolbar_view->bounds().origin();
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  gfx::Point expanded_origin = toolbar_view->bounds().origin();
  EXPECT_EQ(initial_origin, expanded_origin);

  std::u16string tooltip_expanded =
      expand_button->GetRenderedTooltipText(gfx::Point());
  EXPECT_EQ(tooltip_expanded,
            l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_COLLAPSE));

  // Buttons are drawn.
  EXPECT_TRUE(regenerate_button->IsDrawn());
  EXPECT_TRUE(replace_photo_button->IsDrawn());
  EXPECT_TRUE(delete_photo_button->IsDrawn());

  // A manually expanded toolbar does not auto-compact.
  task_environment()->FastForwardBy(kAutoCompactDelay);
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

  // Collapse the toolbar.
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  EXPECT_EQ(expand_button->GetRenderedTooltipText(gfx::Point()),
            tooltip_collapsed);

  // Buttons should be hidden again.
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

  task_environment()->FastForwardBy(kAutoCompactDelay);
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_TRUE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_LT(toolbar_view->width(), collapsed_width);

  ui::test::EventGenerator event_generator(GetRootWindow(widget()),
                                           widget()->GetNativeWindow());
  event_generator.MoveMouseTo(toolbar_view->GetBoundsInScreen().CenterPoint());
  overlay_view()->DeprecatedLayoutImmediately();

  EXPECT_TRUE(expand_button->IsDrawn());
  EXPECT_FALSE(spark_icon->IsDrawn());
  EXPECT_FALSE(regenerate_button->IsDrawn());

  event_generator.MoveMouseTo(gfx::Point(1000, 1000));
  task_environment()->FastForwardBy(kAutoCompactDelay);
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

  task_environment()->FastForwardBy(kAutoCompactDelay);
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
  task_environment()->FastForwardBy(kAutoCompactDelay);
  overlay_view()->DeprecatedLayoutImmediately();
  EXPECT_TRUE(spark_icon->IsDrawn());
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
  verify_button_a11y(IndigoToolbar::kCloseButtonElementId, IDS_CLOSE);

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

}  // namespace indigo
