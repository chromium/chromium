// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/set_shape_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_content_client.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/set_shape/set_shape.mojom.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// The width and height of the app window created in `CreateWindowWidget`.
constexpr int kWindowWidth = 300;
constexpr int kWindowHeight = 300;

std::unique_ptr<views::Widget> CreateWindowWidget(aura::Window* context) {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = context;
  params.bounds = gfx::Rect(0, 0, kWindowWidth, kWindowHeight);
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

// Helper class to set the display mode of a web contents in unit tests.
class FakeWebContentsDelegate : public content::WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* source) override {
    return display_mode_;
  }

  void set_display_mode(blink::mojom::DisplayMode display_mode) {
    display_mode_ = display_mode;
  }

 private:
  // `setShape` requires the unframed display mode, use that by default.
  blink::mojom::DisplayMode display_mode_ =
      blink::mojom::DisplayMode::kUnframed;
};

}  // namespace

class SetShapeServiceImplTest : public AshTestBase {
 public:
  SetShapeServiceImplTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}

  void SetUp() override {
    test_content_client_ = std::make_unique<content::TestContentClient>();
    content::SetContentClient(test_content_client_.get());
    test_browser_client_ =
        std::make_unique<content::TestContentBrowserClient>();
    content::SetBrowserClientForTesting(test_browser_client_.get());

    AshTestBase::SetUp();

    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(&browser_context_);

    // Add the `web_contents_` to `widget_` so that `GetWidget` returns it.
    widget_ = CreateWindowWidget(GetContext());
    widget_->GetNativeWindow()->AddChild(web_contents_->GetNativeView());
    web_contents_->SetDelegate(&fake_delegate_);

    SetShapeServiceImpl::CreateForTesting(web_contents_->GetPrimaryMainFrame(),
                                          remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    remote_.reset();
    web_contents_ = nullptr;
    web_contents_factory_.reset();
    widget_.reset();
    AshTestBase::TearDown();
    content::SetBrowserClientForTesting(nullptr);
    test_browser_client_.reset();
    content::SetContentClient(nullptr);
    test_content_client_.reset();
  }

 protected:
  std::unique_ptr<content::TestContentClient> test_content_client_;
  std::unique_ptr<content::TestContentBrowserClient> test_browser_client_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  FakeWebContentsDelegate fake_delegate_;
  mojo::Remote<blink::mojom::SetShapeService> remote_;
};

TEST_F(SetShapeServiceImplTest, SetShapeCreatesEventTargeter) {
  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Verify that the window has an event targeter.
  EXPECT_TRUE(widget_->GetNativeWindow()->targeter());
}

TEST_F(SetShapeServiceImplTest, SetShapeWithEmptyRectsResetsTargeter) {
  // First set a shape.
  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future1;
  remote_->SetShape(rects, future1.GetCallback());
  EXPECT_EQ(future1.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Then clear it.
  base::test::TestFuture<blink::mojom::SetShapeResult> future2;
  remote_->SetShape({}, future2.GetCallback());
  EXPECT_EQ(future2.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Verify that the window no longer has a custom event targeter.
  EXPECT_FALSE(widget_->GetNativeWindow()->targeter());
}

TEST_F(SetShapeServiceImplTest, SetShapeReturnsNoWindowIfNoWidget) {
  // Remove `web_contents_` from `widget_`.
  widget_->GetNativeWindow()->RemoveChild(web_contents_->GetNativeView());

  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kNoWindow);
}

TEST_F(SetShapeServiceImplTest, SetShapeFailsIfWindowIsNotUnframed) {
  fake_delegate_.set_display_mode(blink::mojom::DisplayMode::kStandalone);

  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kNotUnframed);
}

TEST_F(SetShapeServiceImplTest, SetShapeFailsIfNoRectIs10x10) {
  std::vector<gfx::Rect> rects = {
      gfx::Rect(10, 10, /*width=*/9, /*height=*/9),
      gfx::Rect(20, 20, /*width=*/5, /*height=*/10)};
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->SetShape(rects, /*callback=*/base::DoNothing());
  EXPECT_EQ(
      "SetShape called with invalid shape (no rect meets minimum size "
      "requirement).",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(SetShapeServiceImplTest, SetShapeSucceedsIfAtLeastOneRectIs10x10) {
  std::vector<gfx::Rect> rects = {
      gfx::Rect(10, 10, /*width=*/9, /*height=*/9),
      gfx::Rect(20, 20, /*width=*/10, /*height=*/10)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);
}

TEST_F(SetShapeServiceImplTest, SetShapeFailsIfGivenTooManyRects) {
  std::vector<gfx::Rect> rects(blink::mojom::kMaxSetShapeRects + 1,
                               gfx::Rect(10, 10, 50, 50));

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->SetShape(rects, base::DoNothing());

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "SetShape called with too many rects.");
}

TEST_F(SetShapeServiceImplTest,
       SetShapeFailsIfNoRectMeetsMinimumSizeInWindowBounds) {
  // Rectangles located completely outside window bounds.
  std::vector<gfx::Rect> rects = {
      gfx::Rect(-kWindowWidth - 20, -kWindowHeight - 20, 10, 10),
      gfx::Rect(kWindowWidth + 20, kWindowHeight + 20, 50, 50),
  };

  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kOutOfBounds);
}

TEST_F(SetShapeServiceImplTest,
       SetShapeFailsIfIntersectionWithWindowBoundsIsTooSmall) {
  // Rectangle is only 5px width inside the window.
  std::vector<gfx::Rect> rects = {gfx::Rect(kWindowWidth - 5, 0, 10, 10)};

  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kOutOfBounds);
}

TEST_F(SetShapeServiceImplTest,
       SetShapeSucceedsWithOnePixelToleranceAtWindowBoundary) {
  // Rectangle is 9px inside the window on the bottom-right. Accepted because
  // the service allows 1px tolerance on the bottom/right edges.
  std::vector<gfx::Rect> rects = {
      gfx::Rect(kWindowWidth - 9, kWindowHeight - 9, 10, 10),
  };

  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);
}

TEST_F(SetShapeServiceImplTest,
       SetShapeSucceedsWithRectanglesExceedingWindowBounds) {
  // Rectangle is larger than window bounds, simulates an async window resize.
  std::vector<gfx::Rect> rects = {
      gfx::Rect(0, 0, kWindowWidth + 100, kWindowHeight + 50),
  };

  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);

  ASSERT_TRUE(widget_->GetNativeWindow()->layer()->alpha_shape());
  EXPECT_EQ(*widget_->GetNativeWindow()->layer()->alpha_shape(), rects);
}

TEST_F(SetShapeServiceImplTest,
       SetShapeSucceedsIfPartiallyOutsideWithValidIntersection) {
  // Rectangle is partially outside but has 10px inside the window.
  std::vector<gfx::Rect> rects = {gfx::Rect(-5, 0, 15, 10)};

  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote_->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);

  ASSERT_TRUE(widget_->GetNativeWindow()->layer()->alpha_shape());
  EXPECT_EQ(*widget_->GetNativeWindow()->layer()->alpha_shape(), rects);
}

}  // namespace ash
