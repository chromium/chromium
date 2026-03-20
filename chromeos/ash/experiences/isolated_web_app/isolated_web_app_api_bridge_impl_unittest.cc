// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_bridge_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_content_client.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/chromeos/isolated_web_app_api_bridge.mojom.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

std::unique_ptr<views::Widget> CreateWindowWidget(aura::Window* context) {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = context;
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

}  // namespace

class IsolatedWebAppApiBridgeImplTest : public AshTestBase {
 public:
  IsolatedWebAppApiBridgeImplTest()
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
  }

  void TearDown() override {
    web_contents_ = nullptr;
    web_contents_factory_.reset();
    widget_.reset();
    AshTestBase::TearDown();
    content::SetBrowserClientForTesting(nullptr);
    test_browser_client_.reset();
    content::SetContentClient(nullptr);
    test_content_client_.reset();
  }

  content::RenderFrameHost* render_frame_host() {
    return web_contents_->GetPrimaryMainFrame();
  }

 protected:
  std::unique_ptr<content::TestContentClient> test_content_client_;
  std::unique_ptr<content::TestContentBrowserClient> test_browser_client_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(IsolatedWebAppApiBridgeImplTest, SetShapeCreatesEventTargeter) {
  mojo::Remote<blink::mojom::IsolatedWebAppApiBridge> remote;
  IsolatedWebAppApiBridgeImpl::CreateForTesting(
      render_frame_host(), remote.BindNewPipeAndPassReceiver());

  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Verify that the window has an event targeter.
  EXPECT_TRUE(widget_->GetNativeWindow()->targeter());
}

TEST_F(IsolatedWebAppApiBridgeImplTest, SetShapeWithEmptyRectsResetsTargeter) {
  mojo::Remote<blink::mojom::IsolatedWebAppApiBridge> remote;
  IsolatedWebAppApiBridgeImpl::CreateForTesting(
      render_frame_host(), remote.BindNewPipeAndPassReceiver());

  // First set a shape.
  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future1;
  remote->SetShape(rects, future1.GetCallback());
  EXPECT_EQ(future1.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Then clear it.
  base::test::TestFuture<blink::mojom::SetShapeResult> future2;
  remote->SetShape({}, future2.GetCallback());
  EXPECT_EQ(future2.Get(), blink::mojom::SetShapeResult::kSuccess);

  // Verify that the window no longer has a custom event targeter.
  EXPECT_FALSE(widget_->GetNativeWindow()->targeter());
}

TEST_F(IsolatedWebAppApiBridgeImplTest, SetShapeReturnsNoWindowIfNoWidget) {
  mojo::Remote<blink::mojom::IsolatedWebAppApiBridge> remote;
  IsolatedWebAppApiBridgeImpl::CreateForTesting(
      render_frame_host(), remote.BindNewPipeAndPassReceiver());

  // Remove `web_contents_` from `widget_`.
  widget_->GetNativeWindow()->RemoveChild(web_contents_->GetNativeView());

  std::vector<gfx::Rect> rects = {gfx::Rect(10, 10, 50, 50)};
  base::test::TestFuture<blink::mojom::SetShapeResult> future;
  remote->SetShape(rects, future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::SetShapeResult::kNoWindow);
}

}  // namespace ash
