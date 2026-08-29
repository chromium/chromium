// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/host_override.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

TEST(HostOverrideTest, FromString_Valid) {
  auto override = HostOverride::FromString("localhost.corp.google.com");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("localhost.corp.google.com", override->host);
}

TEST(HostOverrideTest, FromString_IPv6) {
  auto override = HostOverride::FromString("[::1]");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("::1", override->host);
}

TEST(HostOverrideTest, FromString_UnbracketedIPv6) {
  auto override = HostOverride::FromString("::1");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("::1", override->host);
}

TEST(HostOverrideTest, FromString_Empty) {
  EXPECT_FALSE(HostOverride::FromString("").has_value());
}

TEST(HostOverrideTest, ToString) {
  HostOverride domain{"localhost.corp.google.com"};
  EXPECT_EQ("localhost.corp.google.com", domain.ToString());

  HostOverride ipv6{"::1"};
  EXPECT_EQ("[::1]", ipv6.ToString());
}

TEST(HostOverrideTest, Matches) {
  HostOverride override{"localhost.corp.google.com"};
  EXPECT_TRUE(override.Matches(
      GURL("https://localhost.corp.google.com/search?q=test")));
  EXPECT_FALSE(override.Matches(GURL("https://example.com/search?q=test")));

  // Case-insensitivity check.
  EXPECT_TRUE(override.Matches(
      GURL("https://LOCALHOST.CORP.GOOGLE.COM/search?q=test")));

  // IPv6 check.
  HostOverride ipv6{"::1"};
  EXPECT_TRUE(ipv6.Matches(GURL("https://[::1]/search?q=test")));
  EXPECT_FALSE(ipv6.Matches(GURL("https://example.com/search?q=test")));
}

TEST(HostOverrideTest, ApplyToUrl) {
  HostOverride override{"localhost.corp.google.com"};

  GURL url1("https://www.google.com/search?q=test#hash");
  GURL result1 = override.ApplyToUrl(url1);
  EXPECT_EQ("https://localhost.corp.google.com/search?q=test#hash",
            result1.spec());
  EXPECT_EQ("localhost.corp.google.com", result1.host());
}

TEST(HostOverrideTest, EqualityOperator) {
  HostOverride a{"host1"};
  HostOverride b{"host1"};
  HostOverride c{"host2"};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

}  // namespace contextual_tasks
