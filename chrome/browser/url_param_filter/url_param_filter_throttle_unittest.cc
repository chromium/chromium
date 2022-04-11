// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filter_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/url_param_filter/cross_otr_observer.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace url_param_filter {

// Tests the UrlParamFilterThrottle, which is currently a very thin wrapper
// around the url_param_filter::FilterUrl() function. Coverage is accordingly
// somewhat less thorough than that seen in url_param_filterer_unittest.
class UrlParamFilterThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  constexpr static const char kHistogramName[] =
      "Navigation.UrlParamFilter.FilteredParamCountExperimental";

  UrlParamFilterThrottleTest() {
    std::string encoded_classification =
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{"source.xyz", {"plzblock"}},
             {"redirect.abc", {"plzblockredirect"}},
             {"redirect2.abc", {"plzblockredirect2"}}},
            {{"destination.xyz", {"plzblock1"}}});
    // With the flag set, the URL should be filtered.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification}});
  }

  void CreateCrossOtrState() {
    NavigateParams params(profile(), GURL("https://www.foo.com"),
                          ui::PAGE_TRANSITION_LINK);

    params.started_from_context_menu = true;
    params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
    content::WebContents* contents = web_contents();
    CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UrlParamFilterThrottleTest, ShouldCreateThrottleNullContents) {
  network::ResourceRequest resource_request;
  resource_request.is_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(nullptr, resource_request,
                                              &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleTest, ShouldCreateThrottleNotCrossOtr) {
  network::ResourceRequest resource_request;
  resource_request.is_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(web_contents(), resource_request,
                                              &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleTest, ShouldCreateThrottleNotMainFrame) {
  CreateCrossOtrState();
  network::ResourceRequest resource_request;
  resource_request.is_main_frame = false;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(web_contents(), resource_request,
                                              &result);

  ASSERT_EQ(result.size(), 0u);
}

TEST_F(UrlParamFilterThrottleTest, ShouldCreateThrottleTrueCase) {
  CreateCrossOtrState();
  network::ResourceRequest resource_request;
  resource_request.is_main_frame = true;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  UrlParamFilterThrottle::MaybeCreateThrottle(web_contents(), resource_request,
                                              &result);

  ASSERT_EQ(result.size(), 1u);
}

TEST_F(UrlParamFilterThrottleTest, WillStartRequestNullInitiatorNoChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(absl::nullopt);
  resource_request->url = expected_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered no parameters.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 0);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleTest, WillStartRequestInitiatorChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL destination_url = GURL("https://no-rule.xyz?asdf=1&plzblock=1");
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  absl::optional<url::Origin> initiator =
      absl::make_optional(url::Origin::Create(GURL("https://source.xyz")));
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(initiator);
  resource_request->url = destination_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}
TEST_F(UrlParamFilterThrottleTest,
       WillStartRequestNoInitiatorDestinationChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL destination_url = GURL("https://destination.xyz?asdf=1&plzblock1=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(absl::nullopt);
  resource_request->url = destination_url;

  bool defer = false;

  histograms.ExpectTotalCount(kHistogramName, 0);
  throttle.WillStartRequest(resource_request.get(), &defer);

  // Filtered one parameter.
  ASSERT_EQ(histograms.GetTotalSum(kHistogramName), 1);
  ASSERT_EQ(resource_request->url, expected_url);
  ASSERT_FALSE(defer);
}

TEST_F(UrlParamFilterThrottleTest, WillRedirectRequestNullInitiatorNoChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(absl::nullopt);
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

TEST_F(UrlParamFilterThrottleTest, WillRedirectRequestInitiatorChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL destination_url = GURL("https://no-rule.xyz?asdf=1&plzblock=1");
  GURL expected_url = GURL("https://no-rule.xyz?asdf=1");
  absl::optional<url::Origin> initiator =
      absl::make_optional(url::Origin::Create(GURL("https://source.xyz")));
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(initiator);
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
TEST_F(UrlParamFilterThrottleTest,
       WillRedirectRequestNoInitiatorDestinationChanges) {
  base::HistogramTester histograms;

  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  GURL destination_url = GURL("https://destination.xyz?asdf=1&plzblock1=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle = UrlParamFilterThrottle(absl::nullopt);
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

TEST_F(UrlParamFilterThrottleTest, MultipleRedirects) {
  std::unique_ptr<net::RedirectInfo> redirect_info =
      std::make_unique<net::RedirectInfo>();
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // The chain is: source.xyz-->redirect.abc-->redirect2.abc-->destination.xyz.
  resource_request->url = GURL("https://redirect.abc?plzblock=1");
  GURL expected_first_intermediate_url = GURL("https://redirect.abc");
  GURL redirect_url = GURL("https://redirect2.abc?plzblockredirect=1");
  GURL expected_second_intermediate_url = GURL("https://redirect2.abc");
  GURL destination_url =
      GURL("https://destination.xyz?asdf=1&plzblockredirect2=1");
  GURL expected_url = GURL("https://destination.xyz?asdf=1");
  UrlParamFilterThrottle throttle =
      UrlParamFilterThrottle(url::Origin::Create(GURL("https://source.xyz")));
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

}  // namespace url_param_filter
