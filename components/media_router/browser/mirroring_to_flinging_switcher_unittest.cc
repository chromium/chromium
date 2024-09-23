// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_to_flinging_switcher.h"

#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media_router {

class TestMediaRouterFactory : public MediaRouterFactory {
 public:
  TestMediaRouterFactory() = default;
  ~TestMediaRouterFactory() override = default;

  void ShutdownForBrowserContext(content::BrowserContext* context) {
    BrowserContextShutdown(context);
    BrowserContextDestroyed(context);
  }

  MOCK_METHOD(KeyedService*,
              BuildServiceInstanceFor,
              (content::BrowserContext * context),
              (const));
};

class TestWebContentsPresentationManager
    : public WebContentsPresentationManager {
 public:
  TestWebContentsPresentationManager() = default;
  ~TestWebContentsPresentationManager() override = default;

  void SetDefaultPresentationRequest(
      const content::PresentationRequest& request) {
    default_presentation_request_ = request;
  }

  bool HasDefaultPresentationRequest() const override {
    return default_presentation_request_.has_value();
  }

  const content::PresentationRequest& GetDefaultPresentationRequest()
      const override {
    return *default_presentation_request_;
  }

  base::WeakPtr<media_router::WebContentsPresentationManager> GetWeakPtr()
      override {
    return weak_factory_.GetWeakPtr();
  }

  // Not relevant for this test.
  void AddObserver(content::PresentationObserver* observer) override {}
  void RemoveObserver(content::PresentationObserver* observer) override {}
  std::vector<MediaRoute> GetMediaRoutes() override { return {}; }
  void OnPresentationResponse(
      const content::PresentationRequest& presentation_request,
      mojom::RoutePresentationConnectionPtr connection,
      const RouteRequestResult& result) override {}

 private:
  std::optional<content::PresentationRequest> default_presentation_request_;
  base::WeakPtrFactory<TestWebContentsPresentationManager> weak_factory_{this};
};

class MirroringToFlingingSwitcherTest : public testing::Test {
 public:
  MirroringToFlingingSwitcherTest() = default;

  ~MirroringToFlingingSwitcherTest() override {
    media_router_factory_.ShutdownForBrowserContext(&browser_context_);
  }

  void SetUp() override {
    media_router_ = static_cast<MockMediaRouter*>(
        media_router_factory_.SetTestingFactoryAndUse(
            &browser_context_, base::BindRepeating(&MockMediaRouter::Create)));
    ASSERT_TRUE(
        MediaRouterFactory::GetApiForBrowserContextIfExists(&browser_context_));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&browser_context_));

    presentation_manager_ =
        std::make_unique<TestWebContentsPresentationManager>();
    media_router::WebContentsPresentationManager::SetTestInstance(
        presentation_manager_.get());
  }

  content::FrameTreeNodeId GetNewTabSource() {
    return web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestMediaRouterFactory media_router_factory_;
  raw_ptr<MockMediaRouter, DanglingUntriaged> media_router_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestWebContentsPresentationManager> presentation_manager_;
};

TEST_F(MirroringToFlingingSwitcherTest, SwitchToFlingingFaliure) {
  // No switch to flinging is expected as DefaultPresentationRequest is not set.
  EXPECT_CALL(*media_router_, JoinRouteInternal).Times(0);
  SwitchToFlingingIfPossible(GetNewTabSource());
  task_environment_.RunUntilIdle();
}

// This test checks if a request for switching to flinging is sent. It doesn't
// actually verify if the flinging session started.
TEST_F(MirroringToFlingingSwitcherTest, SwitchToFlinging) {
  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://defaultpresentation.com/")},
      url::Origin::Create(GURL("http://origin/")));
  presentation_manager_->SetDefaultPresentationRequest(presentation_request);

  ASSERT_TRUE(presentation_manager_->HasDefaultPresentationRequest());
  const auto source_id =
      MediaSource::ForPresentationUrl(presentation_request.presentation_urls[0])
          .id();
  EXPECT_CALL(*media_router_,
              JoinRouteInternal(source_id, kAutoJoinPresentationId,
                                presentation_request.frame_origin,
                                web_contents_.get(), _, base::TimeDelta()));

  // Switch to flinging request is expected to be sent.
  SwitchToFlingingIfPossible(GetNewTabSource());
  task_environment_.RunUntilIdle();
}

}  // namespace media_router
