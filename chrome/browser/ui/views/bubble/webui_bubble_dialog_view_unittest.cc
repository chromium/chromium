// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {
class TestBubbleContentsWrapper : public BubbleContentsWrapper {
 public:
  explicit TestBubbleContentsWrapper(Profile* profile)
      : BubbleContentsWrapper(GURL(""), profile, 0, true, true) {}
  void ReloadWebContents() override {}
};
}  // namespace

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
    contents_wrapper_ =
        std::make_unique<TestBubbleContentsWrapper>(profile_.get());

    auto bubble_view = std::make_unique<WebUIBubbleDialogView>(
        anchor_widget_->GetContentsView(), contents_wrapper_.get());
    bubble_view_ = bubble_view.get();
    bubble_widget_ =
        BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  }
  void TearDown() override {
    bubble_widget_->CloseNow();
    anchor_widget_.reset();
    contents_wrapper_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  WebUIBubbleDialogView* bubble_dialog_view() { return bubble_view_; }
  Widget* bubble_widget() { return bubble_widget_; }
  BubbleContentsWrapper* contents_wrapper() {
    return bubble_view_->get_contents_wrapper_for_testing();
  }
  views::WebView* web_view() { return bubble_view_->web_view(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<TestBubbleContentsWrapper> contents_wrapper_;
  raw_ptr<Widget> bubble_widget_ = nullptr;
  raw_ptr<WebUIBubbleDialogView> bubble_view_ = nullptr;
};

TEST_F(WebUIBubbleDialogViewTest, BubbleRespondsToWebViewPreferredSizeChanges) {
  constexpr gfx::Size web_view_initial_size(100, 100);
  bubble_dialog_view()->ResizeDueToAutoResize(nullptr, web_view_initial_size);
  const gfx::Size widget_initial_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_initial_size.width(), web_view_initial_size.width());
  EXPECT_GE(widget_initial_size.height(), web_view_initial_size.height());

  // Resize the webview.
  constexpr gfx::Size web_view_final_size(200, 200);
  bubble_dialog_view()->ResizeDueToAutoResize(nullptr, web_view_final_size);

  // Ensure the bubble resizes as expected.
  const gfx::Size widget_final_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  EXPECT_LT(widget_initial_size.width(), widget_final_size.width());
  EXPECT_LT(widget_initial_size.height(), widget_final_size.height());
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_final_size.width(), web_view_final_size.width());
  EXPECT_GE(widget_final_size.height(), web_view_final_size.height());
}

TEST_F(WebUIBubbleDialogViewTest, ClearContentsWrapper) {
  EXPECT_NE(nullptr, contents_wrapper());
  EXPECT_NE(nullptr, web_view()->web_contents());
  EXPECT_EQ(bubble_dialog_view(), contents_wrapper()->GetHost().get());

  bubble_dialog_view()->ClearContentsWrapper();

  EXPECT_EQ(nullptr, contents_wrapper());
  EXPECT_EQ(nullptr, web_view()->web_contents());
}

TEST_F(WebUIBubbleDialogViewTest, CloseUIClearsContentsWrapper) {
  EXPECT_NE(nullptr, contents_wrapper());
  EXPECT_NE(nullptr, web_view()->web_contents());
  EXPECT_EQ(bubble_dialog_view(), contents_wrapper()->GetHost().get());

  bubble_dialog_view()->CloseUI();
  EXPECT_TRUE(bubble_widget()->IsClosed());

  EXPECT_EQ(nullptr, contents_wrapper());
  EXPECT_EQ(nullptr, web_view()->web_contents());
}

TEST_F(WebUIBubbleDialogViewTest, GetAnchorRectWithProvidedAnchorRect) {
  UniqueWidgetPtr anchor_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Init(std::move(params));
  auto profile_ = std::make_unique<TestingProfile>();
  auto contents_wrapper =
      std::make_unique<TestBubbleContentsWrapper>(profile_.get());

  gfx::Rect anchor(666, 666, 0, 0);
  auto bubble_dialog = std::make_unique<WebUIBubbleDialogView>(
      anchor_widget->GetContentsView(), contents_wrapper.get(), anchor);

  EXPECT_EQ(bubble_dialog->GetAnchorRect(), anchor);

  anchor_widget->CloseNow();
}

}  // namespace test
}  // namespace views
