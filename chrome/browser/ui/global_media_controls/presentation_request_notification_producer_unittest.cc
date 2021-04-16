// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
media_router::MediaRoute CreateMediaRoute(
    media_router::MediaRoute::Id route_id) {
  media_router::MediaRoute media_route(route_id,
                                       media_router::MediaSource("source_id"),
                                       "sink_id", "description", true, true);
  media_route.set_controller_type(media_router::RouteControllerType::kGeneric);
  return media_route;
}
}  // namespace

class PresentationRequestNotificationProducerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationRequestNotificationProducerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::MainThreadType::UI) {}
  ~PresentationRequestNotificationProducerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);
    ChromeRenderViewHostTestHarness::SetUp();

    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));
    notification_service_ =
        std::make_unique<MediaNotificationService>(profile(), false);
    notification_producer_ =
        notification_service_->presentation_request_notification_producer_
            .get();
  }

  void TearDown() override {
    notification_service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SimulateStartPresentationContextCreated() {
    auto context = std::make_unique<media_router::StartPresentationContext>(
        content::PresentationRequest(
            main_rfh()->GetGlobalFrameRoutingId(),
            {GURL("http://example.com"), GURL("http://example2.com")},
            url::Origin::Create(GURL("http://google.com"))),
        base::BindOnce(
            &PresentationRequestNotificationProducerTest::RequestSuccess,
            base::Unretained(this)),
        base::BindOnce(
            &PresentationRequestNotificationProducerTest::RequestError,
            base::Unretained(this)));
    notification_producer_->OnStartPresentationContextCreated(
        std::move(context));
  }

  void SimulateMediaRouteChanged(
      const std::vector<media_router::MediaRoute>& routes) {
    notification_producer_->OnMediaRoutesChanged(routes);
  }

  MOCK_METHOD3(RequestSuccess,
               void(const blink::mojom::PresentationInfo&,
                    media_router::mojom::RoutePresentationConnectionPtr,
                    const media_router::MediaRoute&));
  MOCK_METHOD1(RequestError,
               void(const blink::mojom::PresentationError& error));

 protected:
  std::unique_ptr<MediaNotificationService> notification_service_;
  PresentationRequestNotificationProducer* notification_producer_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PresentationRequestNotificationProducerTest,
       HideItemOnMediaRoutesChanged) {
  SimulateStartPresentationContextCreated();
  SimulateMediaRouteChanged({CreateMediaRoute("id")});
  EXPECT_FALSE(notification_service_->HasOpenDialog());
  task_environment()->RunUntilIdle();
}

TEST_F(PresentationRequestNotificationProducerTest, DismissNotification) {
  SimulateStartPresentationContextCreated();
  auto item = notification_producer_->GetNotificationItem();
  ASSERT_TRUE(item);

  notification_producer_->OnContainerDismissed(item->id());
  EXPECT_FALSE(notification_producer_->GetNotificationItem());
}
