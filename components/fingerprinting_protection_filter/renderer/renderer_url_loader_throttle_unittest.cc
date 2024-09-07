// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/test/task_environment.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace fingerprinting_protection_filter {

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  MOCK_METHOD2(CancelWithError, void(int, std::string_view));
  MOCK_METHOD0(Resume, void());
};

class MockRendererURLLoaderThrottle : public RendererURLLoaderThrottle {
 public:
  explicit MockRendererURLLoaderThrottle(RendererAgent* renderer_agent)
      : RendererURLLoaderThrottle(renderer_agent,
                                  /*local_frame_token=*/std::nullopt) {}

 protected:
  // Simplify URL list matching.
  bool ShouldAllowRequest() override {
    return GetCurrentURL() != GURL("https://blocked.com/");
  }
};

class RendererURLLoaderThrottleTest : public ::testing::Test {
 protected:
  RendererURLLoaderThrottleTest()
      : renderer_agent_(/*render_frame=*/nullptr, /*ruleset_dealer=*/nullptr) {
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    // Initialize the throttle with a valid `RendererAgent` that doesn't do
    // anything.
    throttle_ =
        std::make_unique<MockRendererURLLoaderThrottle>(&renderer_agent_);
    throttle_->set_delegate(throttle_delegate_.get());
  }

  ~RendererURLLoaderThrottleTest() override = default;

  network::ResourceRequest GetResourceRequest(
      const GURL& url,
      network::mojom::RequestDestination destination) {
    network::ResourceRequest request;
    request.url = url;
    request.destination = destination;
    return request;
  }

  void SetActivationLevel(
      subresource_filter::mojom::ActivationLevel activation_level) {
    subresource_filter::mojom::ActivationState activation_state;
    activation_state.activation_level = activation_level;
    throttle_->OnActivationComputed(activation_state);
  }

  base::test::TaskEnvironment message_loop_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  RendererAgent renderer_agent_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferHttpsImageUrl) {
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

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferChromeUrl) {
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferIframeUrl) {
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

TEST_F(RendererURLLoaderThrottleTest,
       DefersHttpsScriptUrlWhenWaitingForActivation) {
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

TEST_F(RendererURLLoaderThrottleTest,
       DoesNotDeferHttpsScriptUrlWhenActivationComputed) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDisabled);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(RendererURLLoaderThrottleTest, ResumesSafeUrlLoad) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  // Don't set activation before the request starts so that it will be
  // deferred.
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kEnabled);
  EXPECT_CALL(*throttle_delegate_, Resume());
  // `Resume()` is called by posting a task.
  message_loop_.RunUntilIdle();

  // Reset `defer` - the throttle will not modify it except when it decides to
  // defer.
  defer = false;
  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(RendererURLLoaderThrottleTest, BlocksMatchingUrlLoad) {
  GURL url("https://blocked.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  // Don't set activation before the request starts so that it will be
  // deferred.
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kEnabled);
  EXPECT_CALL(*throttle_delegate_,
              CancelWithError(testing::Eq(net::ERR_BLOCKED_BY_CLIENT),
                              testing::Eq("FingerprintingProtection")));
  // `CancelWithError()` is called by posting a task.
  message_loop_.RunUntilIdle();
}

TEST_F(RendererURLLoaderThrottleTest,
       ResumesMatchingUrlLoadWithDisabledActivation) {
  GURL url("https://blocked.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  // Don't set activation before the request starts so that it will be
  // deferred.
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDisabled);
  EXPECT_CALL(*throttle_delegate_, Resume());
  // `Resume()` is called by posting a task.
  message_loop_.RunUntilIdle();

  // Reset `defer` - the throttle will not modify it except when it decides to
  // defer.
  defer = false;
  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(RendererURLLoaderThrottleTest,
       ResumesMatchingUrlLoadWithDryRunActivation) {
  GURL url("https://blocked.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  // Don't set activation before the request starts so that it will be
  // deferred.
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDryRun);
  EXPECT_CALL(*throttle_delegate_, Resume());
  // `Resume()` is called by posting a task.
  message_loop_.RunUntilIdle();

  // Reset `defer` - the throttle will not modify it except when it decides to
  // defer.
  defer = false;
  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

}  // namespace fingerprinting_protection_filter
