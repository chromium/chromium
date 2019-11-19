// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_redirect {

TEST(SubresourceRedirectURL, ProperlyChangesURL) {
  EXPECT_EQ(GetSubresourceURLForURL(GURL("https://www.test.com/test.jpg")),
            GURL("https://"
                 "jy6r5d5zc3n6juvq35nveinxzyuk4n4wndppyli5x5ycmrza36fa."
                 "litepages.googlezip.net/"
                 "i?u=https%3A%2F%2Fwww.test.com%2Ftest.jpg"));
}

TEST(SubresourceRedirectURL, ProperlyHandlesFragment) {
  EXPECT_EQ(GetSubresourceURLForURL(GURL("https://www.test.com/test.jpg#test")),
            GURL("https://"
                 "jy6r5d5zc3n6juvq35nveinxzyuk4n4wndppyli5x5ycmrza36fa."
                 "litepages.googlezip.net/"
                 "i?u=https%3A%2F%2Fwww.test.com%2Ftest.jpg#test"));
}

TEST(SubresourceRedirectURL, ProperlyHandlesSetPort) {
  EXPECT_EQ(GetSubresourceURLForURL(GURL("https://www.test.com:4444/test.jpg")),
            GURL("https://flm6clfkawcjb2bw5cnrrcdf4fkoliileuljcc23lahdet75ouqq."
                 "litepages.googlezip.net/i?u=https%3A%2F%2Fwww.test."
                 "com%3A4444%2Ftest.jpg"));
}

TEST(SubresourceRedirectURL, ProperlyHandlesQueryParams) {
  EXPECT_EQ(GetSubresourceURLForURL(
                GURL("https://www.test.com/test.jpg?color=yellow")),
            GURL("https://"
                 "jy6r5d5zc3n6juvq35nveinxzyuk4n4wndppyli5x5ycmrza36fa."
                 "litepages.googlezip.net/"
                 "i?u=https%3A%2F%2Fwww.test.com%2Ftest.jpg%3Fcolor%3Dyellow"));
}

TEST(SubresourceRedirectURL, ProperlyHandlesMultipleQueryParams) {
  EXPECT_EQ(GetSubresourceURLForURL(
                GURL("https://www.test.com/test.jpg?color=yellow&name=test")),
            GURL("https://"
                 "jy6r5d5zc3n6juvq35nveinxzyuk4n4wndppyli5x5ycmrza36fa."
                 "litepages.googlezip.net/"
                 "i?u=https%3A%2F%2Fwww.test.com%2Ftest.jpg%3Fcolor%3Dyellow%"
                 "26name%3Dtest"));
}

TEST(SubresourceRedirectURL, ProperlyHandlesQueryParamsWithFragments) {
  EXPECT_EQ(GetSubresourceURLForURL(
                GURL("https://www.test.com/test.jpg?color=yellow#test")),
            GURL("https://"
                 "jy6r5d5zc3n6juvq35nveinxzyuk4n4wndppyli5x5ycmrza36fa."
                 "litepages.googlezip.net/"
                 "i?u=https%3A%2F%2Fwww.test.com%2Ftest.jpg%3Fcolor%3Dyellow"
                 "#test"));
}

// Currently redirects are not supported for HTTP subresources, but there is
// potential to add them in the future.
TEST(SubresourceRedirectURL, ProperlyChangesHTTPURL) {
  EXPECT_EQ(GetSubresourceURLForURL(GURL("http://www.test.com/test.jpg")),
            GURL("https://bc6pgqmtr6nlicooao2zh77svcptncpwfolwlgbrop6gqnr6ck3q."
                 "litepages.googlezip.net/i?u=http%3A%2F%2Fwww.test.com%2Ftest."
                 "jpg"));
}

}  // namespace subresource_redirect
