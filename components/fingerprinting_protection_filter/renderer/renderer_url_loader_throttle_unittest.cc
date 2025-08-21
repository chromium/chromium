// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/variations/variations_switches.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::mojom::ActivationLevel;
using ::subresource_filter::mojom::ActivationState;
using ::testing::_;
using ::testing::IsEmpty;

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  MOCK_METHOD2(CancelWithError, void(int error_code, std::string_view message));
  MOCK_METHOD0(Resume, void());
};

class RendererURLLoaderThrottleTest : public ::testing::Test {
 protected:
  RendererURLLoaderThrottleTest() = default;
  ~RendererURLLoaderThrottleTest() override = default;

  void SetUp() override {
    SetTestRulesetToDisallowURLsWithPathSuffix("blocked.com/tracker.js");
    ASSERT_TRUE(ruleset_);

    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    throttle_ = RendererURLLoaderThrottle::CreateForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault(), ruleset_,
        base::BindLambdaForTesting(
            [&]() -> RendererAgent* { return &renderer_agent_; }));
    throttle_->set_delegate(throttle_delegate_.get());
  }

  void TearDown() override {
    throttle_.reset();
    ruleset_ = nullptr;
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(std::string_view suffix) {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_ = subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
        subresource_filter::testing::TestRuleset::Open(
            test_ruleset_pair.indexed));
  }

  network::ResourceRequest GetResourceRequest(
      const GURL& url,
      network::mojom::RequestDestination destination) {
    network::ResourceRequest request;
    request.url = url;
    request.destination = destination;
    return request;
  }

  void SetActivationLevel(ActivationLevel activation_level) {
    ActivationState activation_state;
    activation_state.activation_level = activation_level;
    renderer_agent_.ActivateForNextCommittedLoad(activation_state.Clone());
  }

  void RunUntilActivationReceived(ActivationLevel activation_level) {
    EXPECT_TRUE(base::test::RunUntil([this, activation_level]() {
      auto current_activation = throttle_->GetCurrentActivation();
      return current_activation.has_value() &&
             current_activation.value() == activation_level;
    }));
  }

  base::test::TaskEnvironment task_environment_;
  MockRendererAgent renderer_agent_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_ = nullptr;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
  scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset_ =
      nullptr;
  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
};

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferSafeRequest) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com/image.jpg");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kImage);

  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "FingerprintingProtection.SubresourceLoad.TotalDeferTime"),
              IsEmpty());
}

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferChromeUrl) {
  base::HistogramTester histogram_tester;
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);

  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "FingerprintingProtection.SubresourceLoad.TotalDeferTime"),
              IsEmpty());
}

TEST_F(RendererURLLoaderThrottleTest, DoesNotDeferIframeUrl) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kIframe);

  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "FingerprintingProtection.SubresourceLoad.TotalDeferTime"),
              IsEmpty());
}

TEST_F(RendererURLLoaderThrottleTest,
       DefersScriptRequestWhenWaitingForActivation) {
  GURL url("https://example.com/script.js");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
}

TEST_F(RendererURLLoaderThrottleTest, DefersRedirectWhenWaitingForActivation) {
  GURL url("chrome://placeholder");
  bool defer = false;
  // The request starts as a resource that will be ignored by the throttle.
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  auto response_head = network::mojom::URLResponseHead::New();
  GURL redirect_url("https://blocked.com/tracker.js");
  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirect_url;
  throttle_->WillRedirectRequest(&redirect_info, *response_head, &defer,
                                 nullptr, nullptr, nullptr);
  EXPECT_TRUE(defer);
}

TEST_F(RendererURLLoaderThrottleTest,
       DoesNotDeferHttpsScriptUrlWhenActivationComputed) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kDisabled);
  RunUntilActivationReceived(
      subresource_filter::mojom::ActivationLevel::kDisabled);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
}

// Regression test for https://crbug.com/436470071.
TEST_F(RendererURLLoaderThrottleTest,
       DoesNotDeferHttpsScriptRedirectWhenActivationComputed) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  EXPECT_CALL(*throttle_delegate_, Resume());
  SetActivationLevel(subresource_filter::mojom::ActivationLevel::kEnabled);
  RunUntilActivationReceived(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  defer = false;
  auto response_head = network::mojom::URLResponseHead::New();
  GURL redirect_url("https://blocked.com/tracker.js");
  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirect_url;
  EXPECT_CALL(*throttle_delegate_,
              CancelWithError(net::ERR_BLOCKED_BY_FINGERPRINTING_PROTECTION,
                              "FingerprintingProtection"));
  throttle_->WillRedirectRequest(&redirect_info, *response_head, &defer,
                                 nullptr, nullptr, nullptr);
  EXPECT_FALSE(defer);
}

TEST_F(RendererURLLoaderThrottleTest, ResumesSafeUrlLoad) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com/script.js");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  EXPECT_CALL(*throttle_delegate_, Resume());
  SetActivationLevel(ActivationLevel::kEnabled);
  RunUntilActivationReceived(ActivationLevel::kEnabled);

  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.SubresourceLoad.TotalDeferTime.Allowed", 1);
}

TEST_F(RendererURLLoaderThrottleTest, BlocksMatchingUrlLoad) {
  base::HistogramTester histogram_tester;
  GURL url("https://blocked.com/tracker.js");

  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  EXPECT_CALL(renderer_agent_, OnSubresourceDisallowed(url, _));
  EXPECT_CALL(*throttle_delegate_,
              CancelWithError(net::ERR_BLOCKED_BY_FINGERPRINTING_PROTECTION,
                              "FingerprintingProtection"));
  SetActivationLevel(ActivationLevel::kEnabled);
  RunUntilActivationReceived(ActivationLevel::kEnabled);

  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.SubresourceLoad.TotalDeferTime.Disallowed", 1);
}

TEST_F(RendererURLLoaderThrottleTest,
       BlocksMatchingUrlLoadThatStartsAfterActivationReceived) {
  base::HistogramTester histogram_tester;
  GURL url("https://blocked.com/tracker.js");

  SetActivationLevel(ActivationLevel::kEnabled);
  RunUntilActivationReceived(ActivationLevel::kEnabled);

  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  EXPECT_CALL(*throttle_delegate_,
              CancelWithError(net::ERR_BLOCKED_BY_FINGERPRINTING_PROTECTION,
                              "FingerprintingProtection"));
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  // Expect no histogram despite the blocked resource since the request was
  // never deferred.
  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.SubresourceLoad.TotalDeferTime.Disallowed", 0);
}

TEST_F(RendererURLLoaderThrottleTest,
       ResumesMatchingUrlLoadWithDisabledActivation) {
  base::HistogramTester histogram_tester;
  GURL url("https://blocked.com/tracker.js");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  EXPECT_CALL(*throttle_delegate_, Resume());
  SetActivationLevel(ActivationLevel::kDisabled);
  RunUntilActivationReceived(ActivationLevel::kDisabled);

  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.SubresourceLoad.TotalDeferTime."
      "ActivationDisabled",
      1);
}

TEST_F(RendererURLLoaderThrottleTest,
       ResumesMatchingUrlLoadWithDryRunActivation) {
  base::HistogramTester histogram_tester;
  GURL url("https://blocked.com/tracker.js");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  EXPECT_CALL(*throttle_delegate_, Resume());
  SetActivationLevel(ActivationLevel::kDryRun);
  RunUntilActivationReceived(ActivationLevel::kDryRun);

  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.SubresourceLoad.TotalDeferTime.WouldDisallow",
      1);
}

TEST_F(RendererURLLoaderThrottleTest, Localhost_DefersOnlyWhenBenchmarking) {
  GURL url("http://localhost/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      variations::switches::kEnableBenchmarking);

  auto throttle2 = RendererURLLoaderThrottle::CreateForTesting(
      base::SingleThreadTaskRunner::GetCurrentDefault(), ruleset_,
      base::BindLambdaForTesting(
          [&]() -> RendererAgent* { return &renderer_agent_; }));
  throttle2->set_delegate(throttle_delegate_.get());

  defer = false;
  throttle2->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
}

}  // namespace fingerprinting_protection_filter
