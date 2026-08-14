// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_navigation_error_data.h"

#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

TEST(EnterpriseProxyNavigationErrorDataTest,
     NullNavigationHandle_ReturnsNullAndFalse) {
  EXPECT_FALSE(
      EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(nullptr));
  EXPECT_EQ(EnterpriseProxyNavigationErrorData::Get(nullptr), nullptr);

  // Delegate should handle a null navigation handle without crashing.
  EnterpriseProxyErrorDataDelegate delegate(nullptr);
  delegate.AttachDisguisedErrorData(
      EnterpriseProxyErrorData(GURL("https://destination.example.com/test"),
                               GURL("https://proxy.example.com:443"), 403));
}

TEST(EnterpriseProxyNavigationErrorDataTest, AttachAndRetrieveErrorData) {
  content::MockNavigationHandle mock_navigation_handle;
  EXPECT_FALSE(EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(
      &mock_navigation_handle));

  EnterpriseProxyErrorDataDelegate delegate(&mock_navigation_handle);
  delegate.AttachDisguisedErrorData(
      EnterpriseProxyErrorData(GURL("https://destination.example.com/test"),
                               GURL("https://proxy.example.com:443"), 403));

  EXPECT_TRUE(EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(
      &mock_navigation_handle));
  EXPECT_NE(delegate.GetDisguisedErrorData(), nullptr);
  auto* error_data =
      EnterpriseProxyNavigationErrorData::Get(&mock_navigation_handle);
  ASSERT_TRUE(error_data);
  EXPECT_EQ(delegate.GetDisguisedErrorData(), error_data);
  EXPECT_EQ(error_data->destination_url(),
            GURL("https://destination.example.com/test"));
  EXPECT_EQ(error_data->proxy_url(), GURL("https://proxy.example.com:443"));
  EXPECT_EQ(error_data->error_code(), 403);
}

TEST(EnterpriseProxyNavigationErrorDataTest,
     AttachAndRetrieveWithInvalidOrEmptyUrls) {
  content::MockNavigationHandle mock_navigation_handle;

  EnterpriseProxyErrorDataDelegate delegate(&mock_navigation_handle);
  delegate.AttachDisguisedErrorData(
      EnterpriseProxyErrorData(GURL("invalid-url"), GURL(), 500));

  EXPECT_TRUE(EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(
      &mock_navigation_handle));
  auto* error_data =
      EnterpriseProxyNavigationErrorData::Get(&mock_navigation_handle);
  ASSERT_TRUE(error_data);
  EXPECT_FALSE(error_data->destination_url().is_valid());
  EXPECT_TRUE(error_data->proxy_url().is_empty());
  EXPECT_EQ(error_data->error_code(), 500);
}

TEST(EnterpriseProxyNavigationErrorDataTest,
     AttachAndRetrieveDifferentErrorCodes) {
  for (int error_code : {500, 502, 503, 504}) {
    content::MockNavigationHandle mock_navigation_handle;
    EnterpriseProxyErrorDataDelegate delegate(&mock_navigation_handle);
    delegate.AttachDisguisedErrorData(EnterpriseProxyErrorData(
        GURL("https://destination.example.com/"),
        GURL("https://proxy.example.com:443"), error_code));

    EXPECT_TRUE(EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(
        &mock_navigation_handle));
    auto* error_data =
        EnterpriseProxyNavigationErrorData::Get(&mock_navigation_handle);
    ASSERT_TRUE(error_data);
    EXPECT_EQ(error_data->error_code(), error_code);
  }
}

}  // namespace enterprise_net
