// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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

class IndigoToolbarTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();

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
    views::ViewsTestBase::TearDown();
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

 private:
  std::unique_ptr<views::LayoutProvider> layout_provider_;
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

  // Buttons are initially not drawn.
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());

  // Expand the toolbar.
  gfx::Point initial_origin = toolbar_view->bounds().origin();
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  gfx::Point expanded_origin = toolbar_view->bounds().origin();
  EXPECT_EQ(initial_origin, expanded_origin);

  // Buttons are drawn.
  EXPECT_TRUE(regenerate_button->IsDrawn());
  EXPECT_TRUE(replace_photo_button->IsDrawn());
  EXPECT_TRUE(delete_photo_button->IsDrawn());

  // Interact with expanded buttons.
  EXPECT_CALL(delegate, OnRegenerate(toolbar.get())).Times(1);
  views::test::ButtonTestApi(regenerate_button).NotifyDefaultMouseClick();
  EXPECT_CALL(delegate, OnReplaceOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(replace_photo_button).NotifyDefaultMouseClick();
  EXPECT_CALL(delegate, OnDeleteOriginalPhoto(toolbar.get())).Times(1);
  views::test::ButtonTestApi(delete_photo_button).NotifyDefaultMouseClick();

  // Collapse the toolbar.
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();

  // Buttons should be hidden again.
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());
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

}  // namespace indigo
