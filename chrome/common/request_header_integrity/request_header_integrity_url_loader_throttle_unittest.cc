// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/google_header_names.h"
#include "chrome/test/base/scoped_channel_override.h"
#endif

namespace request_header_integrity {

namespace {

class RequestHeaderIntegrityURLLoaderThrottleTest : public testing::Test {
 public:
  RequestHeaderIntegrityURLLoaderThrottleTest()
      : throttle_(std::make_unique<RequestHeaderIntegrityURLLoaderThrottle>()) {
  }
  RequestHeaderIntegrityURLLoaderThrottleTest(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;
  RequestHeaderIntegrityURLLoaderThrottleTest& operator=(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottleTest() override = default;

 protected:
  RequestHeaderIntegrityURLLoaderThrottle& throttle() { return *throttle_; }

 private:
  std::unique_ptr<RequestHeaderIntegrityURLLoaderThrottle> throttle_;
};

}  // namespace

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, NonGoogleSite) {
  network::ResourceRequest request;
  request.url = GURL("https://www.somesite.com/");

  ASSERT_TRUE(request.cors_exempt_headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_TRUE(request.cors_exempt_headers.IsEmpty());
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
  std::vector<std::string> to_be_removed_request_headers;
  net::HttpRequestHeaders modified_request_headers;
  net::HttpRequestHeaders modified_cors_exempt_request_headers;

  throttle().WillRedirectRequest(
      &redirect_info, response_head, &defer, &to_be_removed_request_headers,
      &modified_request_headers, &modified_cors_exempt_request_headers);
  // Check size only as header macros are not guaranteed on unbranded builds.
  EXPECT_EQ(4u, to_be_removed_request_headers.size());
  EXPECT_EQ(0u, modified_request_headers.GetHeaderVector().size());
  EXPECT_EQ(0u, modified_cors_exempt_request_headers.GetHeaderVector().size());
}

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, RedirectToGoogle) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  std::vector<std::string> to_be_removed_request_headers;
  net::HttpRequestHeaders modified_request_headers;
  net::HttpRequestHeaders modified_cors_exempt_request_headers;

  throttle().WillRedirectRequest(
      &redirect_info, response_head, &defer, &to_be_removed_request_headers,
      &modified_request_headers, &modified_cors_exempt_request_headers);
  EXPECT_EQ(0u, to_be_removed_request_headers.size());
  EXPECT_EQ(0u, modified_request_headers.GetHeaderVector().size());
  // Check size only as header macros are not guaranteed on unbranded builds.
  EXPECT_EQ(3u, modified_cors_exempt_request_headers.GetHeaderVector().size());
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
  std::vector<std::string> to_be_removed_request_headers;
  net::HttpRequestHeaders modified_request_headers;
  net::HttpRequestHeaders modified_cors_exempt_request_headers;

  throttle().WillRedirectRequest(
      &redirect_info, response_head, &defer, &to_be_removed_request_headers,
      &modified_request_headers, &modified_cors_exempt_request_headers);
  EXPECT_THAT(to_be_removed_request_headers,
              testing::UnorderedElementsAreArray({
                  CHANNEL_NAME_HEADER_NAME,
                  COPYRIGHT_HEADER_NAME,
                  LASTCHANGE_YEAR_HEADER_NAME,
                  VALIDATE_HEADER_NAME,
              }));
  EXPECT_EQ(0u, modified_request_headers.GetHeaderVector().size());
  EXPECT_EQ(0u, modified_cors_exempt_request_headers.GetHeaderVector().size());
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
  std::vector<std::string> to_be_removed_request_headers;
  net::HttpRequestHeaders modified_request_headers;
  net::HttpRequestHeaders modified_cors_exempt_request_headers;

  throttle().WillRedirectRequest(
      &redirect_info, response_head, &defer, &to_be_removed_request_headers,
      &modified_request_headers, &modified_cors_exempt_request_headers);
  EXPECT_EQ(0u, to_be_removed_request_headers.size());
  EXPECT_EQ(0u, modified_request_headers.GetHeaderVector().size());
  EXPECT_EQ(4u, modified_cors_exempt_request_headers.GetHeaderVector().size());
  EXPECT_TRUE(
      modified_cors_exempt_request_headers.HasHeader(CHANNEL_NAME_HEADER_NAME));
  EXPECT_TRUE(modified_cors_exempt_request_headers.HasHeader(
      LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(
      modified_cors_exempt_request_headers.HasHeader(VALIDATE_HEADER_NAME));
  EXPECT_TRUE(
      modified_cors_exempt_request_headers.HasHeader(COPYRIGHT_HEADER_NAME));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS) && \
        // !BUILDFLAG(IS_ANDROID)

}  // namespace request_header_integrity
