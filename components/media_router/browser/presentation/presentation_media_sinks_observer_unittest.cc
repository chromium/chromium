// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_media_sinks_observer.h"

#include <memory>

#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/mock_screen_availability_listener.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace media_router {

namespace {
constexpr char kOrigin[] = "https://google.com";
}  // namespace

class PresentationMediaSinksObserverTest : public ::testing::Test {
 public:
  PresentationMediaSinksObserverTest()
      : listener_(GURL("http://example.com/presentation.html")) {}

  PresentationMediaSinksObserverTest(
      const PresentationMediaSinksObserverTest&) = delete;
  PresentationMediaSinksObserverTest& operator=(
      const PresentationMediaSinksObserverTest&) = delete;

  ~PresentationMediaSinksObserverTest() override = default;

  void SetUp() override {
    EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).WillOnce(Return(true));
    observer_ = std::make_unique<PresentationMediaSinksObserver>(
        &router_, &listener_,
        MediaSource::ForPresentationUrl(
            GURL("http://example.com/presentation.html")),
        url::Origin::Create(GURL(kOrigin)));
    EXPECT_TRUE(observer_->Init());
  }

  void TearDown() override {
    EXPECT_CALL(router_, UnregisterMediaSinksObserver(_));
    observer_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 public:
  MockMediaRouter router_;
  MockScreenAvailabilityListener listener_;
  std::unique_ptr<PresentationMediaSinksObserver> observer_;
};

TEST_F(PresentationMediaSinksObserverTest, AvailableScreens) {
  std::vector<MediaSink> result;
  result.push_back(CreateCastSink("sinkId", "Sink"));

  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(
                             blink::mojom::ScreenAvailability::AVAILABLE))
      .Times(1);
  observer_->OnSinksReceived(result);
}

TEST_F(PresentationMediaSinksObserverTest, NoAvailableScreens) {
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(
                             blink::mojom::ScreenAvailability::UNAVAILABLE))
      .Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
}

TEST_F(PresentationMediaSinksObserverTest, ConsecutiveResults) {
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(
                             blink::mojom::ScreenAvailability::UNAVAILABLE))
      .Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // Does not propagate result to |listener_| since result is same.
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // |listener_| should get result since it changed to true.
  std::vector<MediaSink> result;
  result.push_back(CreateCastSink("sinkId", "Sink"));

  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(
                             blink::mojom::ScreenAvailability::AVAILABLE))
      .Times(1);
  observer_->OnSinksReceived(result);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // Does not propagate result to |listener_| since result is same.
  result.push_back(CreateCastSink("sinkId2", "Sink 2"));
  observer_->OnSinksReceived(result);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // |listener_| should get result since it changed to false.
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(
                             blink::mojom::ScreenAvailability::UNAVAILABLE))
      .Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));
}

}  // namespace media_router
