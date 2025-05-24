// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_dialog_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/media_router/browser/android/media_router_dialog_controller_android.h"
#endif

using ::testing::_;
using ::testing::Return;

namespace media_router {

class TestMediaRouterDialogController : public MediaRouterDialogController {
 public:
  explicit TestMediaRouterDialogController(content::WebContents* initiator)
      : MediaRouterDialogController(initiator) {}
  ~TestMediaRouterDialogController() override = default;

  bool IsShowingMediaRouterDialog() const override { return has_dialog_; }
  void CreateMediaRouterDialog(
      MediaRouterDialogActivationLocation activation_location) override {
    has_dialog_ = true;
  }
  void CloseMediaRouterDialog() override { has_dialog_ = false; }

 private:
  bool has_dialog_ = false;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(ActivateContents, void(content::WebContents* web_contents));
};

class MediaRouterDialogControllerTest
    : public content::RenderViewHostTestHarness {
 public:
  MOCK_METHOD3(RequestSuccess,
               void(const blink::mojom::PresentationInfo&,
                    mojom::RoutePresentationConnectionPtr,
                    const MediaRoute&));
  MOCK_METHOD1(RequestError,
               void(const blink::mojom::PresentationError& error));

 protected:
  MediaRouterDialogControllerTest() = default;
  ~MediaRouterDialogControllerTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    web_contents_delegate_ = std::make_unique<MockWebContentsDelegate>();
    web_contents()->SetDelegate(web_contents_delegate_.get());
    dialog_controller_ =
        std::make_unique<TestMediaRouterDialogController>(web_contents());
  }

  void TearDown() override {
    dialog_controller_.reset();
    web_contents_delegate_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  bool ShowMediaRouterDialogForPresentation() {
    return dialog_controller_->ShowMediaRouterDialogForPresentation(
        std::make_unique<StartPresentationContext>(
            content::PresentationRequest(
                {1, 2},
                {GURL("http://example.com"), GURL("http://example2.com")},
                url::Origin::Create(GURL("http://google.com"))),
            base::BindOnce(&MediaRouterDialogControllerTest::RequestSuccess,
                           base::Unretained(this)),
            base::BindOnce(&MediaRouterDialogControllerTest::RequestError,
                           base::Unretained(this))));
  }

  std::unique_ptr<TestMediaRouterDialogController> dialog_controller_;
  std::unique_ptr<MockWebContentsDelegate> web_contents_delegate_;
};

#if BUILDFLAG(IS_ANDROID)
// The non-Android implementation is tested in
// MediaRouterDialogControllerViewsTest.
TEST_F(MediaRouterDialogControllerTest, CreateForWebContents) {
  MediaRouterDialogController::SetGetOrCreate(
      base::BindRepeating([](content::WebContents* web_contents) {
        DCHECK(web_contents);
        MediaRouterDialogController* controller = nullptr;
        MediaRouterDialogControllerAndroid::CreateForWebContents(web_contents);
        controller =
            MediaRouterDialogControllerAndroid::FromWebContents(web_contents);
        return controller;
      }));
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents());
  ASSERT_NE(dialog_controller, nullptr);
  // Returns the same value the second time.
  ASSERT_EQ(
      dialog_controller,
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents()));
}
#endif

TEST_F(MediaRouterDialogControllerTest, ShowAndHideDialog) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation::TOOLBAR));
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialog() should return false.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_FALSE(dialog_controller_->ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation::TOOLBAR));

  dialog_controller_->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller_->IsShowingMediaRouterDialog());

  // Once the dialog is hidden, ShowMediaRouterDialog() should return true
  // again.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation::TOOLBAR));
}

TEST_F(MediaRouterDialogControllerTest, ShowDialogForPresentation) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(ShowMediaRouterDialogForPresentation());
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialogForPresentation() should
  // return false.
  EXPECT_FALSE(ShowMediaRouterDialogForPresentation());

  // The error callback is invoked automatically if StartPresentationContext is
  // destroyed without a response.
  EXPECT_CALL(*this, RequestError(_)).Times(1);
}

TEST_F(MediaRouterDialogControllerTest, StartPresentationContext) {
  auto context = std::make_unique<StartPresentationContext>(
      content::PresentationRequest(
          {1, 2}, {GURL("http://example.com"), GURL("http://example2.com")},
          url::Origin::Create(GURL("http://google.com"))),
      base::BindOnce(&MediaRouterDialogControllerTest::RequestSuccess,
                     base::Unretained(this)),
      base::BindOnce(&MediaRouterDialogControllerTest::RequestError,
                     base::Unretained(this)));

  MediaRoute route("routeId", MediaSource::ForTab(1), "sinkId", "Description",
                   false);
  auto result = RouteRequestResult::FromSuccess(route, "presentationId");

  EXPECT_CALL(*this, RequestSuccess(_, _, _)).Times(1);
  EXPECT_CALL(*this, RequestError(_)).Times(0);
  context->HandleRouteResponse(nullptr, *result);
}

}  // namespace media_router
