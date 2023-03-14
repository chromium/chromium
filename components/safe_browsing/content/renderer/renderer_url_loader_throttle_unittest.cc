// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class FakeSafeBrowsing : public mojom::SafeBrowsing {
 public:
  FakeSafeBrowsing() = default;

  void CreateCheckerAndCheck(
      int32_t render_frame_id,
      mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      int32_t load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      bool originated_from_service_worker,
      CreateCheckerAndCheckCallback callback) override {
    if (should_delay_callback_) {
      pending_callback_ = std::move(callback);
      receiver_ = std::move(receiver);
    } else {
      std::move(callback).Run(/*slow_check_notifier=*/mojo::NullReceiver(),
                              /*proceed=*/true, /*show_interstitial=*/false,
                              /*did_perform_real_time_check=*/false,
                              /*did_check_allowlist=*/false);
    }
  }

  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override {
    NOTREACHED();
  }

  void RestartDelayedCallback() {
    ASSERT_TRUE(should_delay_callback_);
    std::move(pending_callback_)
        .Run(/*slow_check_notifier=*/mojo::NullReceiver(),
             /*proceed=*/true, /*show_interstitial=*/false,
             /*did_perform_real_time_check=*/false,
             /*did_check_allowlist=*/false);
  }

  void EnableDelayCallback() { should_delay_callback_ = true; }

 private:
  bool should_delay_callback_ = false;
  mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver_;
  CreateCheckerAndCheckCallback pending_callback_;
};

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {}
  void Resume() override {}
};

class SBRendererUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBRendererUrlLoaderThrottleTest() : mojo_receiver_(&safe_browsing_) {
    feature_list_.InitAndEnableFeature(kSafeBrowsingSkipImageCssFont);
    mojo_receiver_.Bind(safe_browsing_remote_.BindNewPipeAndPassReceiver());
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    throttle_ = std::make_unique<RendererURLLoaderThrottle>(
        safe_browsing_remote_.get(), MSG_ROUTING_NONE);
    throttle_->set_delegate(throttle_delegate_.get());
  }

  network::ResourceRequest GetResourceRequest(
      const GURL& url,
      network::mojom::RequestDestination destination) {
    network::ResourceRequest request;
    request.url = url;
    request.destination = destination;
    return request;
  }

  base::test::TaskEnvironment message_loop_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  FakeSafeBrowsing safe_browsing_;
  mojo::Receiver<mojom::SafeBrowsing> mojo_receiver_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(SBRendererUrlLoaderThrottleTest, DefersHttpsUrl) {
  safe_browsing_.EnableDelayCallback();
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferHttpsImageUrl) {
  safe_browsing_.EnableDelayCallback();
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kImage);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferChromeUrl) {
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_DoesNotDefer) {
  base::HistogramTester histograms;
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  message_loop_.FastForwardBy(base::Milliseconds(200));
  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);

  histograms.ExpectUniqueTimeSample("SafeBrowsing.RendererThrottle.TotalDelay2",
                                    base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBRendererUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_DoesNotDeferFromCache) {
  base::HistogramTester histograms;
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  message_loop_.FastForwardBy(base::Milliseconds(200));
  auto response_head = network::mojom::URLResponseHead::New();
  // Set up a "cache" response.
  response_head->was_fetched_via_cache = true;
  response_head->network_accessed = false;
  throttle_->WillProcessResponse(url, response_head.get(), &defer);

  histograms.ExpectUniqueTimeSample("SafeBrowsing.RendererThrottle.TotalDelay2",
                                    base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromCache",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_F(SBRendererUrlLoaderThrottleTest, VerifyTotalDelayHistograms_Defer) {
  base::HistogramTester histograms;
  safe_browsing_.EnableDelayCallback();
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);

  message_loop_.FastForwardBy(base::Milliseconds(200));
  safe_browsing_.RestartDelayedCallback();
  message_loop_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample("SafeBrowsing.RendererThrottle.TotalDelay2",
                                    base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBRendererUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_DeferFromCache) {
  base::HistogramTester histograms;
  safe_browsing_.EnableDelayCallback();
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  // Set up a "cache" response.
  response_head->was_fetched_via_cache = true;
  response_head->network_accessed = false;
  throttle_->WillProcessResponse(url, response_head.get(), &defer);

  message_loop_.FastForwardBy(base::Milliseconds(200));
  safe_browsing_.RestartDelayedCallback();
  message_loop_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample("SafeBrowsing.RendererThrottle.TotalDelay2",
                                    base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromCache",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.RendererThrottle.TotalDelay2.FromNetwork", 0);
}

class SBRendererUrlLoaderThrottleDisableSkipImageCssFontTest
    : public SBRendererUrlLoaderThrottleTest {
 public:
  SBRendererUrlLoaderThrottleDisableSkipImageCssFontTest() {
    feature_list_.InitAndDisableFeature(kSafeBrowsingSkipImageCssFont);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SBRendererUrlLoaderThrottleDisableSkipImageCssFontTest,
       DefersHttpsImageUrl) {
  safe_browsing_.EnableDelayCallback();
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kImage);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
}

}  // namespace safe_browsing
