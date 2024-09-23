// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include <string_view>

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
      const std::optional<blink::LocalFrameToken>& frame_token,
      mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      int32_t load_flags,
      bool has_user_gesture,
      bool originated_from_service_worker,
      CreateCheckerAndCheckCallback callback) override {
    if (should_delay_callback_) {
      pending_callback_ = std::move(callback);
      receiver_ = std::move(receiver);
    } else {
      std::move(callback).Run(/*proceed=*/true, /*show_interstitial=*/false);
    }
  }

  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override {
    NOTREACHED_IN_MIGRATION();
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
                       std::string_view custom_reason) override {}
  void Resume() override {}
};

class SBRendererUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBRendererUrlLoaderThrottleTest() : mojo_receiver_(&safe_browsing_) {
    mojo_receiver_.Bind(safe_browsing_remote_.BindNewPipeAndPassReceiver());
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    throttle_ = std::make_unique<RendererURLLoaderThrottle>(
        safe_browsing_remote_.get(), std::nullopt);
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
  FakeSafeBrowsing safe_browsing_;
  mojo::Receiver<mojom::SafeBrowsing> mojo_receiver_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferHttpsImageUrl) {
  base::HistogramTester histograms;
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

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferHttpsScriptUrl) {
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
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferChromeUrl) {
  base::HistogramTester histograms;
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferIframeUrl) {
  base::HistogramTester histograms;
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kIframe);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

}  // namespace safe_browsing
