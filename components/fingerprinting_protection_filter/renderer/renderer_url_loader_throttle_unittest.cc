// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"
#include "components/subresource_filter/core/common/load_policy.h"
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

  void CancelWithError(int error_code, std::string_view message) override {
    if (error_code == net::ERR_BLOCKED_BY_CLIENT &&
        message == "FingerprintingProtection") {
      // Only accept calls with the expected error code and message.
      cancel_called_ = true;
    }
  }

  void Resume() override { resume_called_ = true; }

  bool WasCancelCalled() { return cancel_called_; }

  bool WasResumeCalled() { return resume_called_; }

 private:
  bool cancel_called_ = false;
  bool resume_called_ = false;
};

class MockRendererURLLoaderThrottle : public RendererURLLoaderThrottle {
 public:
  explicit MockRendererURLLoaderThrottle()
      : RendererURLLoaderThrottle(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            /*local_frame_token=*/std::nullopt) {}

  void InjectRendererAgent(base::WeakPtr<RendererAgent> renderer_agent) {
    SetRendererAgentForTesting(renderer_agent);
  }

 protected:
  // Simplify URL list matching.
  bool ShouldAllowRequest(subresource_filter::LoadPolicy load_policy) override {
    return GetCurrentURL() != GURL("https://blocked.com/");
  }
};

class RendererURLLoaderThrottleTest : public ::testing::Test {
 protected:
  RendererURLLoaderThrottleTest()
      : renderer_agent_(/*ruleset_dealer=*/nullptr,
                        /*is_top_level_main_frame=*/true,
                        /*has_valid_opener=*/false) {
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    // Initialize the throttle with a valid `MockRendererAgent` that doesn't do
    // anything.
    throttle_ = std::make_unique<MockRendererURLLoaderThrottle>();
    throttle_->set_delegate(throttle_delegate_.get());
    throttle_->InjectRendererAgent(renderer_agent_.GetWeakPtr());
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

  base::test::TaskEnvironment message_loop_;
  MockRendererAgent renderer_agent_;
  std::unique_ptr<MockRendererURLLoaderThrottle> throttle_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferHttpsImageUrl) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kImage);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

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
  EXPECT_FALSE(defer);

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
  EXPECT_FALSE(defer);

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
  EXPECT_FALSE(defer);

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
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kEnabled);
  auto* throttle_delegate_ptr = throttle_delegate_.get();
  EXPECT_TRUE(base::test::RunUntil([throttle_delegate_ptr]() {
    return throttle_delegate_ptr->WasResumeCalled();
  }));

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
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kEnabled);
  auto* throttle_delegate_ptr = throttle_delegate_.get();
  EXPECT_TRUE(base::test::RunUntil([throttle_delegate_ptr]() {
    return throttle_delegate_ptr->WasCancelCalled();
  }));
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
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDisabled);
  auto* throttle_delegate_ptr = throttle_delegate_.get();
  EXPECT_TRUE(base::test::RunUntil([throttle_delegate_ptr]() {
    return throttle_delegate_ptr->WasResumeCalled();
  }));

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
  EXPECT_TRUE(defer);

  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDryRun);
  auto* throttle_delegate_ptr = throttle_delegate_.get();
  EXPECT_TRUE(base::test::RunUntil([throttle_delegate_ptr]() {
    return throttle_delegate_ptr->WasResumeCalled();
  }));

  // Reset `defer` - the throttle will not modify it except when it decides to
  // defer.
  defer = false;
  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

}  // namespace fingerprinting_protection_filter
