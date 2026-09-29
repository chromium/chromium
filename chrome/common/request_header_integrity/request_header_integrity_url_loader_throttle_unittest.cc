// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/request_header_integrity/chrome_companero_loader.h"
#include "chrome/common/request_header_integrity/platform_runtime_headers.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/google_header_names.h"
#include "chrome/test/base/scoped_channel_override.h"
#endif

namespace request_header_integrity {

// Test seams for the protected constructors. Neither class is meant to be
// instantiated outside of tests, so the injection points are only reachable
// through these subclasses.
class TestChromeCompaneroLoader : public ChromeCompaneroLoader {
 public:
  TestChromeCompaneroLoader() = default;
  ~TestChromeCompaneroLoader() = default;

  using ChromeCompaneroLoader::SetCacheForTesting;
};

class TestRequestHeaderIntegrityURLLoaderThrottle
    : public RequestHeaderIntegrityURLLoaderThrottle {
 public:
  explicit TestRequestHeaderIntegrityURLLoaderThrottle(
      ChromeCompaneroLoader& companero_loader)
      : RequestHeaderIntegrityURLLoaderThrottle(companero_loader) {}
  ~TestRequestHeaderIntegrityURLLoaderThrottle() override = default;
};

class RequestHeaderIntegrityURLLoaderThrottleTest : public testing::Test {
 public:
  RequestHeaderIntegrityURLLoaderThrottleTest()
      : throttle_(std::make_unique<TestRequestHeaderIntegrityURLLoaderThrottle>(
            loader_)) {}
  RequestHeaderIntegrityURLLoaderThrottleTest(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;
  RequestHeaderIntegrityURLLoaderThrottleTest& operator=(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottleTest() override = default;

 protected:
  TestChromeCompaneroLoader& loader() { return loader_; }
  RequestHeaderIntegrityURLLoaderThrottle& throttle() { return *throttle_; }

 private:
  TestChromeCompaneroLoader loader_;
  std::unique_ptr<TestRequestHeaderIntegrityURLLoaderThrottle> throttle_;
};

namespace {

constexpr char kMainResourceHistogram[] =
    "Security.RequestHeaderIntegrity.DynamicHeaderPresent.MainResource";
constexpr char kSubresourceHistogram[] =
    "Security.RequestHeaderIntegrity.DynamicHeaderPresent.Subresource";
constexpr char kPrefetchHistogram[] =
    "Security.RequestHeaderIntegrity.DynamicHeaderPresent.Prefetch";

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, NonGoogleSite) {
  base::HistogramTester histograms;
  network::ResourceRequest request;
  request.url = GURL("https://www.somesite.com/");

  ASSERT_TRUE(request.cors_exempt_headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_TRUE(request.cors_exempt_headers.IsEmpty());
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest,
       DynamicHeaderAbsentWithoutToken) {
  base::HistogramTester histograms;
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");
  request.destination = network::mojom::RequestDestination::kDocument;

  bool ignored = false;
  throttle().WillStartRequest(&request, &ignored);
  histograms.ExpectUniqueSample(kMainResourceHistogram, false, 1);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest,
       PrefetchRecordsPrefetchHistogram) {
  base::HistogramTester histograms;
  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders cors_exempt_headers;

  RequestHeaderIntegrityURLLoaderThrottle::
      ModifyRequestIntegrityHeadersForPrefetch(GURL("https://www.google.com/"),
                                               removed_headers,
                                               cors_exempt_headers);

  EXPECT_TRUE(removed_headers.empty());
  // The prefetch path reads the process-wide ChromeCompaneroLoader, so only
  // the number of samples is checked.
  histograms.ExpectTotalCount(kPrefetchHistogram, 1);
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, GoogleSite) {
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");

  ASSERT_TRUE(request.cors_exempt_headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  // Check size only as header macros are not guaranteed on unbranded builds.
  EXPECT_EQ(3u, request.cors_exempt_headers.GetHeaderVector().size());
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, RedirectToNonGoogle) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.somesite.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);
  // Check size only as header macros are not guaranteed on unbranded builds.
  EXPECT_EQ(4u, headers_update_params.removed_headers.size());
  EXPECT_EQ(0u,
            headers_update_params.modified_headers.GetHeaderVector().size());
  EXPECT_EQ(0u,
            headers_update_params.modified_cors_exempt_headers.GetHeaderVector()
                .size());
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, RedirectToGoogle) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);
  EXPECT_EQ(0u, headers_update_params.removed_headers.size());
  EXPECT_EQ(0u,
            headers_update_params.modified_headers.GetHeaderVector().size());
  // Check size only as header macros are not guaranteed on unbranded builds.
  EXPECT_EQ(3u,
            headers_update_params.modified_cors_exempt_headers.GetHeaderVector()
                .size());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_ANDROID)
TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, GoogleSiteWithBranding) {
  ASSERT_NE(CHANNEL_NAME_HEADER_NAME, "X-Placeholder-1");
  ASSERT_NE(LASTCHANGE_YEAR_HEADER_NAME, "X-Placeholder-2");
  ASSERT_NE(VALIDATE_HEADER_NAME, "X-Placeholder-3");
  ASSERT_NE(COPYRIGHT_HEADER_NAME, "X-Placeholder-4");

  chrome::ScopedChannelOverride override(
      chrome::ScopedChannelOverride::Channel::kStable);
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");

  ASSERT_TRUE(request.cors_exempt_headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_EQ(4u, request.cors_exempt_headers.GetHeaderVector().size());
  EXPECT_TRUE(request.cors_exempt_headers.HasHeader(CHANNEL_NAME_HEADER_NAME));
  EXPECT_TRUE(
      request.cors_exempt_headers.HasHeader(LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(request.cors_exempt_headers.HasHeader(VALIDATE_HEADER_NAME));
  const std::optional<std::string> copyright =
      request.cors_exempt_headers.GetHeader(COPYRIGHT_HEADER_NAME);
  ASSERT_TRUE(copyright.has_value());
  EXPECT_NE(copyright->find("Copyright"), std::string::npos);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest,
       RedirectToNonGoogleWithBranding) {
  ASSERT_NE(CHANNEL_NAME_HEADER_NAME, "X-Placeholder-1");
  ASSERT_NE(LASTCHANGE_YEAR_HEADER_NAME, "X-Placeholder-2");
  ASSERT_NE(VALIDATE_HEADER_NAME, "X-Placeholder-3");
  ASSERT_NE(COPYRIGHT_HEADER_NAME, "X-Placeholder-4");

  // net::RedirectInfo doesn't record the original URL, only the new URL.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.somesite.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);

  std::vector<std::string> expected_removed = {
      CHANNEL_NAME_HEADER_NAME,
      COPYRIGHT_HEADER_NAME,
      LASTCHANGE_YEAR_HEADER_NAME,
      VALIDATE_HEADER_NAME,
  };
#if defined(INTEGRITY_DYNAMIC_HEADER_1)
  expected_removed.push_back(INTEGRITY_DYNAMIC_HEADER_1);
#endif
#if defined(INTEGRITY_DYNAMIC_HEADER_2)
  expected_removed.push_back(INTEGRITY_DYNAMIC_HEADER_2);
#endif

  EXPECT_THAT(headers_update_params.removed_headers,
              testing::UnorderedElementsAreArray(expected_removed));
  EXPECT_EQ(0u,
            headers_update_params.modified_headers.GetHeaderVector().size());
  EXPECT_EQ(0u,
            headers_update_params.modified_cors_exempt_headers.GetHeaderVector()
                .size());
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest,
       RedirectToGoogleWithBranding) {
  ASSERT_NE(CHANNEL_NAME_HEADER_NAME, "X-Placeholder-1");
  ASSERT_NE(LASTCHANGE_YEAR_HEADER_NAME, "X-Placeholder-2");
  ASSERT_NE(VALIDATE_HEADER_NAME, "X-Placeholder-3");
  ASSERT_NE(COPYRIGHT_HEADER_NAME, "X-Placeholder-4");

  // net::RedirectInfo doesn't record the original URL, only the new URL.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);
  EXPECT_EQ(0u, headers_update_params.removed_headers.size());
  EXPECT_EQ(0u,
            headers_update_params.modified_headers.GetHeaderVector().size());
  EXPECT_EQ(4u,
            headers_update_params.modified_cors_exempt_headers.GetHeaderVector()
                .size());
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      CHANNEL_NAME_HEADER_NAME));
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      VALIDATE_HEADER_NAME));
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      COPYRIGHT_HEADER_NAME));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS) && \
        // !BUILDFLAG(IS_ANDROID)

constexpr char kTestHeaderName[] = "X-Integrity-Header";
constexpr char kTestHeaderValue[] = "mocked_token_value_12345678";

class RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest
    : public RequestHeaderIntegrityURLLoaderThrottleTest {
 protected:
  void SetUp() override {
    loader().SetCacheForTesting(kTestHeaderName, kTestHeaderValue);
  }
};

TEST_F(RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest,
       GoogleSiteWithChromeCompanero) {
  base::HistogramTester histograms;
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");
  request.destination = network::mojom::RequestDestination::kDocument;

  ASSERT_TRUE(request.cors_exempt_headers.IsEmpty());
  bool ignored = false;
  throttle().WillStartRequest(&request, &ignored);

  // Dynamic ChromeCompanero header must be present.
  EXPECT_TRUE(request.cors_exempt_headers.HasHeader(kTestHeaderName));
  EXPECT_EQ(kTestHeaderValue,
            request.cors_exempt_headers.GetHeader(kTestHeaderName));
  histograms.ExpectUniqueSample(kMainResourceHistogram, true, 1);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest,
       SubresourceWithChromeCompanero) {
  base::HistogramTester histograms;
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/script.js");
  request.destination = network::mojom::RequestDestination::kScript;

  bool ignored = false;
  throttle().WillStartRequest(&request, &ignored);

  EXPECT_TRUE(request.cors_exempt_headers.HasHeader(kTestHeaderName));
  histograms.ExpectUniqueSample(kSubresourceHistogram, true, 1);
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest,
       RedirectToNonGoogleWithChromeCompanero) {
  base::HistogramTester histograms;
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.somesite.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::Contains(kTestHeaderName));
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest,
       MainResourceRedirectToGoogleWithChromeCompanero) {
  base::HistogramTester histograms;
  // Start off-Google so that only the redirect attaches the headers.
  network::ResourceRequest request;
  request.url = GURL("https://www.somesite.com/");
  request.destination = network::mojom::RequestDestination::kDocument;
  bool defer = false;
  throttle().WillStartRequest(&request, &defer);
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/");
  network::mojom::URLResponseHead response_head;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);

  EXPECT_EQ(0u, headers_update_params.removed_headers.size());
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      kTestHeaderName));
  EXPECT_EQ(kTestHeaderValue,
            headers_update_params.modified_cors_exempt_headers.GetHeader(
                kTestHeaderName));
  // The redirect is recorded against the resource type of the original
  // request.
  histograms.ExpectUniqueSample(kMainResourceHistogram, true, 1);
  histograms.ExpectTotalCount(kSubresourceHistogram, 0);
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest,
       SubresourceRedirectWithinGoogleWithChromeCompanero) {
  base::HistogramTester histograms;
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/script.js");
  request.destination = network::mojom::RequestDestination::kScript;
  bool defer = false;
  throttle().WillStartRequest(&request, &defer);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/other_script.js");
  network::mojom::URLResponseHead response_head;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);

  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      kTestHeaderName));
  // One sample for the initial request and one for the redirect.
  histograms.ExpectUniqueSample(kSubresourceHistogram, true, 2);
  histograms.ExpectTotalCount(kMainResourceHistogram, 0);
}

// Platform Runtime headers are not applied on Android.
#if !BUILDFLAG(IS_ANDROID)
constexpr char kPlatformRuntimeValue[] = "platform_runtime_value";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kApplyResultHistogram[] =
    "ComponentUpdater.PlatformRuntime.RequestHeaderApplyResult";
#endif

// Covers applying the Platform Runtime headers on the redirect and prefetch
// paths.
class RequestHeaderIntegrityURLLoaderThrottlePlatformRuntimeTest
    : public RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest {
 protected:
  void SetUp() override {
    RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest::SetUp();
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
  }

  void TearDown() override {
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
    RequestHeaderIntegrityURLLoaderThrottleWithCompaneroTest::TearDown();
  }

  void SetPlatformRuntimeHeader(std::string_view name, std::string_view value) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(name, value);
    PlatformRuntimeHeaders::GetInstance().Set(headers);
  }

 private:
  // The apply result histogram is subsampled; record every sample here.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

TEST_F(RequestHeaderIntegrityURLLoaderThrottlePlatformRuntimeTest,
       RedirectToGoogleAppliesPlatformRuntimeHeaders) {
  base::HistogramTester histograms;
  SetPlatformRuntimeHeader(kTestHeaderName, kPlatformRuntimeValue);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);

  // The Platform Runtime value replaces the one the throttle attached.
  EXPECT_EQ(kPlatformRuntimeValue,
            headers_update_params.modified_cors_exempt_headers.GetHeader(
                kTestHeaderName));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  histograms.ExpectUniqueSample(kApplyResultHistogram,
                                PlatformRuntimeApplyResult::kApplied, 1);
#endif
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottlePlatformRuntimeTest,
       RedirectToNonGoogleStripsAndDoesNotReaddPlatformRuntimeHeaders) {
  base::HistogramTester histograms;
  SetPlatformRuntimeHeader(kTestHeaderName, kPlatformRuntimeValue);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.somesite.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle().WillRedirectRequest(&redirect_info, response_head, &defer,
                                 &headers_update_params);

  // The integrity headers are stripped and not added back.
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::Contains(kTestHeaderName));
  EXPECT_TRUE(headers_update_params.modified_cors_exempt_headers.IsEmpty());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  histograms.ExpectUniqueSample(kApplyResultHistogram,
                                PlatformRuntimeApplyResult::kNotApplicable, 1);
#endif
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottlePlatformRuntimeTest,
       PrefetchAppliesPlatformRuntimeHeaders) {
  constexpr char kPrefetchHeaderName[] = "X-Prefetch-Integrity-Header";
  base::HistogramTester histograms;
  SetPlatformRuntimeHeader(kPrefetchHeaderName, kPlatformRuntimeValue);

  // The prefetch path reads the process-wide ChromeCompaneroLoader rather
  // than the test one, so seed a header that an earlier hop attached.
  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader(kPrefetchHeaderName, "original_value");

  RequestHeaderIntegrityURLLoaderThrottle::
      ModifyRequestIntegrityHeadersForPrefetch(GURL("https://www.google.com/"),
                                               removed_headers,
                                               cors_exempt_headers);

  EXPECT_TRUE(removed_headers.empty());
  EXPECT_EQ(kPlatformRuntimeValue,
            cors_exempt_headers.GetHeader(kPrefetchHeaderName));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  histograms.ExpectUniqueSample(kApplyResultHistogram,
                                PlatformRuntimeApplyResult::kApplied, 1);
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace request_header_integrity
