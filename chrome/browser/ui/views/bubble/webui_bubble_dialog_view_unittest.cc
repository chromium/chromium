// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
namespace test {

class WebUIBubbleDialogViewTest : public ChromeViewsTestBase {
 public:
  WebUIBubbleDialogViewTest() = default;
  WebUIBubbleDialogViewTest(const WebUIBubbleDialogViewTest&) = delete;
  WebUIBubbleDialogViewTest& operator=(const WebUIBubbleDialogViewTest&) =
      delete;
  ~WebUIBubbleDialogViewTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    anchor_widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Init(std::move(params));

    auto bubble_view = std::make_unique<WebUIBubbleDialogView>(
        anchor_widget_->GetContentsView(),
        std::make_unique<WebUIBubbleView>(profile_.get()));
    bubble_view_ = bubble_view.get();
    bubble_widget_ =
        BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  }
  void TearDown() override {
    bubble_widget_->CloseNow();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  WebUIBubbleDialogView* bubble_dialog_view() { return bubble_view_; }
  Widget* bubble_widget() { return bubble_widget_; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  views::UniqueWidgetPtr anchor_widget_;
  Widget* bubble_widget_ = nullptr;
  WebUIBubbleDialogView* bubble_view_ = nullptr;
};

TEST_F(WebUIBubbleDialogViewTest, BubbleRespondsToWebViewPreferredSizeChanges) {
  views::WebView* const web_view = bubble_dialog_view()->web_view();
  constexpr gfx::Size web_view_initial_size(100, 100);
  web_view->SetPreferredSize(gfx::Size(100, 100));
  bubble_dialog_view()->OnWebViewSizeChanged();
  const gfx::Size widget_initial_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_initial_size.width(), web_view_initial_size.width());
  EXPECT_GE(widget_initial_size.height(), web_view_initial_size.height());

  // Resize the webview.
  constexpr gfx::Size web_view_final_size(200, 200);
  web_view->SetPreferredSize(web_view_final_size);
  bubble_dialog_view()->OnWebViewSizeChanged();

  // Ensure the bubble resizes as expected.
  const gfx::Size widget_final_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  EXPECT_LT(widget_initial_size.width(), widget_final_size.width());
  EXPECT_LT(widget_initial_size.height(), widget_final_size.height());
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_final_size.width(), web_view_final_size.width());
  EXPECT_GE(widget_final_size.height(), web_view_final_size.height());
}

TEST_F(WebUIBubbleDialogViewTest, RemoveWebViewPassesWebView) {
  // The dialog should initially have a WebView after setup.
  views::WebView* const dialog_web_view = bubble_dialog_view()->web_view();
  EXPECT_NE(nullptr, dialog_web_view);

  // RemoveWebView should pass back the WebView used by the dialog.
  std::unique_ptr<views::WebView> removed_web_view =
      bubble_dialog_view()->RemoveWebView();
  EXPECT_EQ(dialog_web_view, removed_web_view.get());

  // The bubble should not hold on to any old WebView pointer.
  EXPECT_EQ(nullptr, bubble_dialog_view()->web_view());
}

}  // namespace test
}  // namespace views
