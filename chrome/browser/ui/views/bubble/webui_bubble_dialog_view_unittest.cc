// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {
class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile,
                                    bool supports_draggable_regions = false)
      : WebUIContentsWrapper(GURL(""),
                             profile,
                             0,
                             true,
                             true,
                             supports_draggable_regions,
                             "Test") {}
  void ReloadWebContents() override {}

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};
}  // namespace

namespace views {
namespace test {

class WebUIBubbleDialogViewTest : public ChromeViewsTestBase,
                                  public testing::WithParamInterface<bool> {
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

    anchor_widget_ =
        CreateTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    contents_wrapper_ = std::make_unique<TestWebUIContentsWrapper>(
        profile_.get(), /*supports_draggable_regions=*/GetParam());

    auto bubble_view = std::make_unique<WebUIBubbleDialogView>(
        anchor_widget_->GetContentsView(), contents_wrapper_->GetWeakPtr());
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
  WebUIContentsWrapper* contents_wrapper() {
    return bubble_view_->get_contents_wrapper_for_testing();
  }
  views::WebView* web_view() { return bubble_view_->web_view(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<TestWebUIContentsWrapper> contents_wrapper_;
  raw_ptr<Widget, DanglingUntriaged> bubble_widget_ = nullptr;
  raw_ptr<WebUIBubbleDialogView, DanglingUntriaged> bubble_view_ = nullptr;
};

TEST_P(WebUIBubbleDialogViewTest, BubbleRespondsToWebViewPreferredSizeChanges) {
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

TEST_P(WebUIBubbleDialogViewTest, ClearContentsWrapper) {
  EXPECT_NE(nullptr, contents_wrapper());
  EXPECT_NE(nullptr, web_view()->web_contents());
  EXPECT_EQ(bubble_dialog_view(), contents_wrapper()->GetHost().get());

  bubble_dialog_view()->ClearContentsWrapper();

  EXPECT_EQ(nullptr, contents_wrapper());
  EXPECT_EQ(nullptr, web_view()->web_contents());
}

TEST_P(WebUIBubbleDialogViewTest, CloseUIClearsContentsWrapper) {
  EXPECT_NE(nullptr, contents_wrapper());
  EXPECT_NE(nullptr, web_view()->web_contents());
  EXPECT_EQ(bubble_dialog_view(), contents_wrapper()->GetHost().get());

  bubble_dialog_view()->CloseUI();
  EXPECT_TRUE(bubble_widget()->IsClosed());

  EXPECT_EQ(nullptr, contents_wrapper());
  EXPECT_EQ(nullptr, web_view()->web_contents());
}

TEST_P(WebUIBubbleDialogViewTest, GetAnchorRectWithProvidedAnchorRect) {
  UniqueWidgetPtr anchor_widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Init(std::move(params));
  auto profile = std::make_unique<TestingProfile>();
  auto contents_wrapper =
      std::make_unique<TestWebUIContentsWrapper>(profile.get());

  gfx::Rect anchor(666, 666, 0, 0);
  auto bubble_dialog = std::make_unique<WebUIBubbleDialogView>(
      anchor_widget->GetContentsView(), contents_wrapper->GetWeakPtr(), anchor);
  auto* bubble_dialog_ptr = bubble_dialog.get();
  BubbleDialogDelegateView::CreateBubble(std::move(bubble_dialog));

  EXPECT_EQ(bubble_dialog_ptr->GetAnchorRect(), anchor);

  anchor_widget->CloseNow();
}

TEST_P(WebUIBubbleDialogViewTest, DestroyingContentsWrapperDoesNotSegfault) {
  UniqueWidgetPtr anchor_widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Init(std::move(params));
  auto profile = std::make_unique<TestingProfile>();
  auto contents_wrapper =
      std::make_unique<TestWebUIContentsWrapper>(profile.get());

  gfx::Rect anchor(666, 666, 0, 0);
  auto bubble_dialog = std::make_unique<WebUIBubbleDialogView>(
      anchor_widget->GetContentsView(), contents_wrapper->GetWeakPtr(), anchor);

  contents_wrapper.reset();
}

TEST_P(WebUIBubbleDialogViewTest, DraggableRegionIsReflectedInHitTest) {
  if (!GetParam()) {
    GTEST_SKIP() << "Only applicable to draggable bubbles, skipping.";
  }

  // Create the WebUI bubble with an appropriate size.
  bubble_dialog_view()->ResizeDueToAutoResize(nullptr, {400, 400});

  // Perform a hittest with no draggable regions set.
  EXPECT_EQ(HTCLIENT, bubble_widget()->GetNonClientComponent({50, 50}));

  // Set the draggable region and assert the draggable region is reported as
  // part of the non client area.
  std::vector<blink::mojom::DraggableRegionPtr> regions;
  auto region_rect = blink::mojom::DraggableRegion::New();
  region_rect->bounds = {10, 10, 100, 100};
  region_rect->draggable = true;
  regions.push_back(std::move(region_rect));
  contents_wrapper()->DraggableRegionsChanged(
      regions, contents_wrapper()->web_contents());
  EXPECT_EQ(HTCAPTION, bubble_widget()->GetNonClientComponent({50, 50}));
}

TEST_P(WebUIBubbleDialogViewTest, DraggableBubbleRetainsBoundsWhenVisible) {
  if (!GetParam()) {
    GTEST_SKIP() << "Only applicable to draggable bubbles, skipping.";
  }

  // Resize the bubble. The dialog will initially be positioned relative to the
  // anchor.
  EXPECT_FALSE(bubble_widget()->IsVisible());
  bubble_dialog_view()->ResizeDueToAutoResize(nullptr, {400, 400});
  const gfx::Rect initial_bounds = bubble_widget()->GetWindowBoundsInScreen();

  // Show the bubble and reposition the bubble on screen, it should translate
  // correctly.
  bubble_widget()->Show();
  EXPECT_TRUE(bubble_widget()->IsVisible());
  constexpr gfx::Vector2d kMoveVector = {100, 100};
  bubble_widget()->SetBounds(initial_bounds + kMoveVector);
  const gfx::Rect new_bounds_pre_resize =
      bubble_widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(initial_bounds + kMoveVector, new_bounds_pre_resize);

  // Update the bubble size. The bubble's size should update but it should
  // remain at its new position.
  bubble_dialog_view()->ResizeDueToAutoResize(nullptr, {500, 500});
  const gfx::Rect new_bounds_post_resize =
      bubble_widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(new_bounds_pre_resize.origin(), new_bounds_post_resize.origin());
  EXPECT_NE(initial_bounds.size(), new_bounds_post_resize.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIBubbleDialogViewTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<
                             WebUIBubbleDialogViewTest::ParamType>& info) {
                           return info.param ? "DraggableRegionsEnabled"
                                             : "DraggableRegionsDisabled";
                         });

}  // namespace test
}  // namespace views
