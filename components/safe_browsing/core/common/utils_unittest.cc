// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SafeBrowsingUtilsTest, TestSetAccessTokenAndClearCookieInResourceRequest) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string access_token = "123";
  // Feature disabled
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests);

    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
    EXPECT_THAT(resource_request->headers.GetHeader(
                    net::HttpRequestHeaders::kAuthorization),
                testing::Optional(std::string("Bearer 123")));
    // Cookies are attached when the feature is disabled.
    EXPECT_EQ(resource_request->credentials_mode,
              network::mojom::CredentialsMode::kInclude);
  }

  // Feature enabled
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests);

    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
    EXPECT_THAT(resource_request->headers.GetHeader(
                    net::HttpRequestHeaders::kAuthorization),
                testing::Optional(std::string("Bearer 123")));
    // Cookies are removed when the feature is enabled.
    EXPECT_EQ(resource_request->credentials_mode,
              network::mojom::CredentialsMode::kOmit);
  }

  // Feature enabled, request omits cookies already.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests);
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
    EXPECT_THAT(resource_request->headers.GetHeader(
                    net::HttpRequestHeaders::kAuthorization),
                testing::Optional(std::string("Bearer 123")));
    // The request should keep omitting cookies.
    EXPECT_EQ(resource_request->credentials_mode,
              network::mojom::CredentialsMode::kOmit);
  }
}

}  // namespace safe_browsing
