// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_autofocus_url_loader_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/variations/net/omnibox_autofocus_http_headers.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace variations {

class OmniboxAutofocusURLLoaderThrottleTest : public testing::Test {
 public:
  OmniboxAutofocusURLLoaderThrottleTest() = default;

  void CreateThrottle() {
    throttle_ = std::make_unique<OmniboxAutofocusURLLoaderThrottle>();
  }

 protected:
  std::unique_ptr<OmniboxAutofocusURLLoaderThrottle> throttle_;
};

struct AppendThrottleTestParam {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  size_t expected_throttles_size;
};

class OmniboxAutofocusURLLoaderThrottleAppendTest
    : public testing::Test,
      public testing::WithParamInterface<AppendThrottleTestParam> {};

TEST_P(OmniboxAutofocusURLLoaderThrottleAppendTest, AppendThrottleIfNeeded) {
  const AppendThrottleTestParam& param = GetParam();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(param.enabled_features,
                                param.disabled_features);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  OmniboxAutofocusURLLoaderThrottle::AppendThrottleIfNeeded(&throttles);
  EXPECT_EQ(throttles.size(), param.expected_throttles_size);
}

INSTANTIATE_TEST_SUITE_P(
    OmniboxAutofocusURLLoaderThrottleTest,
    OmniboxAutofocusURLLoaderThrottleAppendTest,
    testing::ValuesIn(std::vector<AppendThrottleTestParam>{
        // Test with kReportOmniboxAutofocusHeader disabled (by default).
        {/*enabled_features=*/{},
         /*disabled_features=*/{},
         /*expected_throttles_size=*/0u},
        // Test with kReportOmniboxAutofocusHeader disabled (explicitly).
        {/*enabled_features=*/{},
         /*disabled_features=*/{kReportOmniboxAutofocusHeader},
         /*expected_throttles_size=*/0u},
        // Test with kReportOmniboxAutofocusHeader enabled.
        {/*enabled_features=*/{kReportOmniboxAutofocusHeader},
         /*disabled_features=*/{},
#if BUILDFLAG(IS_ANDROID)
         /*expected_throttles_size=*/1u
#else
         /*expected_throttles_size=*/0u
#endif  // BUILDFLAG(IS_ANDROID)
        }}));

#if BUILDFLAG(IS_ANDROID)

TEST_F(OmniboxAutofocusURLLoaderThrottleTest, WillStartRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kReportOmniboxAutofocusHeader, {}},
       {kOmniboxAutofocusOnIncognitoNtp, {{"not_first_tab", "true"}}}},
      {});

  CreateThrottle();
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  bool defer = false;
  throttle_->WillStartRequest(&request, &defer);

  std::optional<std::string> header_value =
      request.cors_exempt_headers.GetHeader(kOmniboxAutofocusHeaderName);
  ASSERT_TRUE(header_value.has_value());
  EXPECT_EQ(*header_value, "4");
  EXPECT_FALSE(defer);
}

TEST_F(OmniboxAutofocusURLLoaderThrottleTest,
       WillRedirectRequest_ToNonGoogleDomain) {
  CreateThrottle();
  net::RedirectInfo redirect_info;
  std::vector<std::string> to_be_removed_headers;
  network::mojom::URLResponseHead response_head;

  // When redirecting to a non-Google URL, the header name should be added to
  // the list of headers to remove.
  redirect_info.new_url = GURL("https://www.not-google.com");
  throttle_->WillRedirectRequest(&redirect_info, response_head, nullptr,
                                 &to_be_removed_headers, nullptr, nullptr);
  EXPECT_EQ(to_be_removed_headers.size(), 1u);
  EXPECT_EQ(to_be_removed_headers[0], kOmniboxAutofocusHeaderName);
}

TEST_F(OmniboxAutofocusURLLoaderThrottleTest,
       WillRedirectRequest_ToGoogleDomain) {
  CreateThrottle();
  net::RedirectInfo redirect_info;
  std::vector<std::string> to_be_removed_headers;
  network::mojom::URLResponseHead response_head;

  // When redirecting to another Google URL, the header should not be removed.
  redirect_info.new_url = GURL("https://www.google.com");
  throttle_->WillRedirectRequest(&redirect_info, response_head, nullptr,
                                 &to_be_removed_headers, nullptr, nullptr);
  EXPECT_TRUE(to_be_removed_headers.empty());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace variations
