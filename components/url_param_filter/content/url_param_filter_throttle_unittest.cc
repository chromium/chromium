// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/content/url_param_filter_throttle.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/url_param_filter/content/cross_otr_observer.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_filter_test_helper.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace url_param_filter {

namespace {
constexpr static const char kHistogramName[] =
    "Navigation.UrlParamFilter.FilteredParamCount";
}  // namespace

// Tests the UrlParamFilterThrottle, which is currently a very thin wrapper
// around the url_param_filter::FilterUrl() function. Coverage is accordingly
// somewhat less thorough than that seen in url_param_filterer_unittest.
class UrlParamFilterThrottleTest : public content::RenderViewHostTestHarness {
 public:
  UrlParamFilterThrottleTest() = default;

 protected:
  std::string encoded_classification =
      CreateBase64EncodedFilterParamClassificationForTesting(
          {{"source.xyz", {"plzblock"}},
           {"redirect.abc", {"plzblockredirect"}},
           {"redirect2.abc", {"plzblockredirect2"}}},
          {{"destination.xyz", {"plzblock1"}}});

  void CreateCrossOtrState() {
    content::WebContents* contents = web_contents();
    CrossOtrObserver::MaybeCreateForWebContents(
        contents, /*is_cross_otr=*/true, /*started_from_context_menu=*/true,
        ui::PAGE_TRANSITION_LINK);
  }
};

class UrlParamFilterThrottleFilteringEnabledTest
    : public UrlParamFilterThrottleTest {
 public:
  UrlParamFilterThrottleFilteringEnabledTest() {
    // With should_filter set true, the URL should be filtered.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification},
         {"should_filter", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       ShouldCreateThrottleNullContents) {
  network::ResourceRequest resource_request;
  resource_request.is_outermost_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(
      /*enabled_by_policy=*/true, nullptr, resource_request, &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       ShouldCreateThrottleNotCrossOtr) {
  network::ResourceRequest resource_request;
  resource_request.is_outermost_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(
      /*enabled_by_policy=*/true, web_contents(), resource_request, &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       ShouldCreateThrottleNotMainFrame) {
  CreateCrossOtrState();
  network::ResourceRequest resource_request;
  resource_request.is_outermost_main_frame = false;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(
      /*enabled_by_policy=*/true, web_contents(), resource_request, &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleTest, ShouldCreateThrottlePolicyDisabled) {
  CreateCrossOtrState();
  network::ResourceRequest resource_request;
  resource_request.is_outermost_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(
      /*enabled_by_policy=*/false, web_contents(), resource_request, &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       ShouldCreateThrottleTrueCase) {
  CreateCrossOtrState();
  network::ResourceRequest resource_request;
  resource_request.is_outermost_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(
      /*enabled_by_policy=*/true, web_contents(), resource_request, &result);

  ASSERT_EQ(result.size(), 1u);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillStartRequestNullInitiatorNoChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  UrlParamFilterThrottle throttle =
      UrlParamFilterThrottle(absl::nullopt, nullptr);
  resource_request->url = expected_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered no parameters.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 0);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillStartRequestInitiatorChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL destination_url = GURL("https://no-rule.xyz?asdf=1&plzblock=1");
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  absl::optional<url::Origin> initiator =
      absl::make_optional(url::Origin::Create(GURL("https://source.xyz")));
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(initiator, nullptr);
  resource_request->url = destination_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}
TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillStartRequestNoInitiatorDestinationChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL destination_url = GURL("https://destination.xyz?asdf=1&plzblock1=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle =
      UrlParamFilterThrottle(absl::nullopt, nullptr);
  resource_request->url = destination_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillRedirectRequestNullInitiatorNoChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  UrlParamFilterThrottle throttle =
      UrlParamFilterThrottle(absl::nullopt, nullptr);
  redirect_info->new_url = expected_url;

  bool defer = false;
  net::HttpRequestHeaders headers;
  std::vector<std::string> removed_headers;
  auto response_head = network::mojom::URLResponseHead::New();

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);

  // Filtered no parameters.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 0);
  ASSERT_EQ(redirect_info->new_url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillRedirectRequestInitiatorChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL destination_url = GURL("https://no-rule.xyz?asdf=1&plzblock=1");
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  absl::optional<url::Origin> initiator =
      absl::make_optional(url::Origin::Create(GURL("https://source.xyz")));
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(initiator, nullptr);
  redirect_info->new_url = destination_url;

  bool defer = false;
  net::HttpRequestHeaders headers;
  std::vector<std::string> removed_headers;
  auto response_head = network::mojom::URLResponseHead::New();

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(redirect_info->new_url, expected_url);
  ASSERT_FALSE(defer);
}
TEST_F(UrlParamFilterThrottleFilteringEnabledTest,
       WillRedirectRequestNoInitiatorDestinationChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL destination_url = GURL("https://destination.xyz?asdf=1&plzblock1=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle =
      UrlParamFilterThrottle(absl::nullopt, nullptr);
  redirect_info->new_url = destination_url;

  bool defer = false;
  net::HttpRequestHeaders headers;
  std::vector<std::string> removed_headers;
  auto response_head = network::mojom::URLResponseHead::New();

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(redirect_info->new_url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleFilteringEnabledTest, MultipleRedirects) {
  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // The chain is:
  // source.xyz-->redirect.abc-->redirect2.abc-->destination.xyz.
  resource_request->url = GURL("https://redirect.abc?plzblock=1");
  GURL expected_first_intermediate_url = GURL("https://redirect.abc");
  GURL redirect_url = GURL("https://redirect2.abc?plzblockredirect=1");
  GURL expected_second_intermediate_url = GURL("https://redirect2.abc");
  GURL destination_url =
      GURL("https://destination.xyz?asdf=1&plzblockredirect2=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(
      url::Origin::Create(GURL("https://source.xyz")), nullptr);
  redirect_info->new_url = redirect_url;

  bool defer = false;
  net::HttpRequestHeaders headers;
  std::vector<std::string> removed_headers;
  auto response_head = network::mojom::URLResponseHead::New();

  throttle.WillStartRequest(resource_request.get(), &defer);
  ASSERT_EQ(resource_request->url, expected_first_intermediate_url);

  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);
  ASSERT_EQ(redirect_info->new_url, expected_second_intermediate_url);
  // The new source should be redirect.abc; the new destination includes
  // plzblockredirect, which should be filtered.
  redirect_info->new_url = destination_url;
  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);

  ASSERT_EQ(redirect_info->new_url, expected_url);
  ASSERT_FALSE(defer);
}

class UrlParamFilterThrottleFilteringDisabledTest
    : public UrlParamFilterThrottleTest {
 public:
  UrlParamFilterThrottleFilteringDisabledTest() {
    // With should_filter set false, the URL shouldn't be filtered.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification},
         {"should_filter", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UrlParamFilterThrottleFilteringDisabledTest,
       DoesntFilter_MultipleRedirects) {
  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // The chain is:
  // source.xyz-->redirect.abc-->redirect2.abc-->destination.xyz.
  resource_request->url = GURL("https://redirect.abc?plzblock=1");
  GURL redirect_url = GURL("https://redirect2.abc?plzblockredirect=1");
  GURL destination_url =
      GURL("https://destination.xyz?asdf=1&plzblockredirect2=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(
      url::Origin::Create(GURL("https://source.xyz")), nullptr);
  redirect_info->new_url = redirect_url;

  bool defer = false;
  net::HttpRequestHeaders headers;
  std::vector<std::string> removed_headers;
  auto response_head = network::mojom::URLResponseHead::New();

  throttle.WillStartRequest(resource_request.get(), &defer);
  // The param isn't filtered by the UrlParamFilterThrottle.
  EXPECT_EQ(resource_request->url, GURL("https://redirect.abc?plzblock=1"));

  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);
  // The param isn't filtered by the UrlParamFilterThrottle.
  EXPECT_EQ(redirect_info->new_url,
            GURL("https://redirect2.abc?plzblockredirect=1"));
  redirect_info->new_url = destination_url;
  throttle.WillRedirectRequest(redirect_info.get(), *response_head, &defer,
                               &removed_headers, &headers, &headers);

  EXPECT_EQ(redirect_info->new_url,
            GURL("https://destination.xyz?asdf=1&plzblockredirect2=1"));
  EXPECT_FALSE(defer);
}

}  // namespace url_param_filter
