// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

class MultistepFilterUtilTest : public testing::Test {
 public:
  MultistepFilterUtilTest() = default;
};

TEST_F(MultistepFilterUtilTest, GetEtldPlusOne) {
  EXPECT_EQ(GetEtldPlusOne(GURL("https://www.example.com")), "example.com");
  EXPECT_EQ(GetEtldPlusOne(GURL("https://sub.example.co.uk")), "example.co.uk");
  EXPECT_EQ(GetEtldPlusOne(GURL("http://localhost:8080")), "localhost");
  EXPECT_EQ(GetEtldPlusOne(GURL("http://127.0.0.1")), "127.0.0.1");
  EXPECT_EQ(GetEtldPlusOne(GURL("http://[::1]")), "[::1]");
  EXPECT_EQ(GetEtldPlusOne(GURL("http://myintranethost")), "myintranethost");
  EXPECT_EQ(GetEtldPlusOne(GURL("")), "");
}

TEST_F(MultistepFilterUtilTest, IsSameDomainOrHost) {
  EXPECT_TRUE(IsSameDomainOrHost(GURL("https://www.example.com"),
                                 GURL("https://example.com/search")));
  EXPECT_TRUE(IsSameDomainOrHost(GURL("https://a.example.com"),
                                 GURL("https://b.example.com")));
  EXPECT_TRUE(IsSameDomainOrHost(GURL("http://localhost:8080"),
                                 GURL("http://localhost:9000")));
  EXPECT_TRUE(
      IsSameDomainOrHost(GURL("http://127.0.0.1"), GURL("http://127.0.0.1")));

  EXPECT_FALSE(IsSameDomainOrHost(GURL("https://example.com"),
                                  GURL("https://test.org")));
  EXPECT_FALSE(IsSameDomainOrHost(GURL("https://example.com"),
                                  GURL("http://127.0.0.1")));
}

TEST_F(MultistepFilterUtilTest, IsUrlSubsumedBy) {
  GURL reference_url("https://example.com/search?q=foo&filter=bar");

  // Identical URL.
  EXPECT_TRUE(IsUrlSubsumedBy(reference_url, reference_url));

  // Subset of parameters.
  EXPECT_TRUE(
      IsUrlSubsumedBy(GURL("https://example.com/search?q=foo"), reference_url));

  // Different base URL (different path).
  EXPECT_FALSE(
      IsUrlSubsumedBy(GURL("https://example.com/other?q=foo"), reference_url));

  // Additional parameters.
  EXPECT_FALSE(IsUrlSubsumedBy(
      GURL("https://example.com/search?q=foo&filter=bar&sort=new"),
      reference_url));

  // Different parameter values.
  EXPECT_FALSE(
      IsUrlSubsumedBy(GURL("https://example.com/search?q=baz"), reference_url));

  // Invalid URLs.
  EXPECT_FALSE(IsUrlSubsumedBy(GURL(), reference_url));
  EXPECT_FALSE(IsUrlSubsumedBy(reference_url, GURL()));
}

}  // namespace multistep_filter
