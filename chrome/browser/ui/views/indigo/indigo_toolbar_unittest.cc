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
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
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

views::Widget* FindToolbarWidget(views::Widget* anchor_widget) {
  views::Widget::Widgets widgets =
      views::Widget::GetAllOwnedWidgets(anchor_widget->GetNativeView());
  for (views::Widget* widget : widgets) {
    if (widget->GetName() == "IndigoToolbar") {
      return widget;
    }
  }
  return nullptr;
}

}  // namespace

class IndigoToolbarTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();

    anchor_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams anchor_params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Init(std::move(anchor_params));
    anchor_widget_->SetBounds(gfx::Rect(100, 100, 800, 600));
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }

  views::View* GetViewFromToolbar(views::Widget* toolbar_widget,
                                  ui::ElementIdentifier id) {
    return toolbar_widget->GetContentsView()->GetViewByElementId(id);
  }

  views::Button* GetButtonFromToolbar(views::Widget* toolbar_widget,
                                      ui::ElementIdentifier id) {
    return views::Button::AsButton(GetViewFromToolbar(toolbar_widget, id));
  }

 private:
  std::unique_ptr<views::LayoutProvider> layout_provider_;
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(IndigoToolbarTest, CloseAndReopen) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(anchor_widget()->GetNativeView());
  EXPECT_CALL(delegate, OnClose(toolbar.get())).Times(2);

  views::Widget* toolbar_widget = FindToolbarWidget(anchor_widget());
  ASSERT_NE(toolbar_widget, nullptr);

  // The close button dismisses the toolbar widget.
  auto* close_button = GetButtonFromToolbar(
      toolbar_widget, IndigoToolbar::kCloseButtonElementId);
  ASSERT_NE(close_button, nullptr);
  views::test::ButtonTestApi(close_button).NotifyDefaultMouseClick();
  EXPECT_EQ(FindToolbarWidget(anchor_widget()), nullptr);

  // Show again
  toolbar->Show(anchor_widget()->GetNativeView());
  toolbar_widget = FindToolbarWidget(anchor_widget());
  EXPECT_NE(toolbar_widget, nullptr);
}

TEST_F(IndigoToolbarTest, ExpandCollapseInteractions) {
  MockIndigoToolbarDelegate delegate;
  auto toolbar = std::make_unique<IndigoToolbar>(&delegate);
  toolbar->Show(anchor_widget()->GetNativeView());
  EXPECT_CALL(delegate, OnClose(toolbar.get())).Times(1);

  views::Widget* toolbar_widget = FindToolbarWidget(anchor_widget());
  ASSERT_NE(toolbar_widget, nullptr);

  auto* expand_button = GetButtonFromToolbar(
      toolbar_widget, IndigoToolbar::kExpandButtonElementId);
  ASSERT_NE(expand_button, nullptr);

  auto* regenerate_button = GetButtonFromToolbar(
      toolbar_widget, IndigoToolbar::kRegenerateButtonElementId);
  ASSERT_NE(regenerate_button, nullptr);

  auto* replace_photo_button = GetButtonFromToolbar(
      toolbar_widget, IndigoToolbar::kReplacePhotoButtonElementId);
  ASSERT_NE(replace_photo_button, nullptr);

  auto* delete_photo_button = GetButtonFromToolbar(
      toolbar_widget, IndigoToolbar::kDeletePhotoButtonElementId);
  ASSERT_NE(delete_photo_button, nullptr);

  // Buttons are initially not drawn.
  EXPECT_FALSE(regenerate_button->IsDrawn());
  EXPECT_FALSE(replace_photo_button->IsDrawn());
  EXPECT_FALSE(delete_photo_button->IsDrawn());

  // Expand the toolbar.
  gfx::Point initial_origin =
      toolbar_widget->GetWindowBoundsInScreen().origin();
  views::test::ButtonTestApi(expand_button).NotifyDefaultMouseClick();
  gfx::Point expanded_origin =
      toolbar_widget->GetWindowBoundsInScreen().origin();
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

}  // namespace indigo
