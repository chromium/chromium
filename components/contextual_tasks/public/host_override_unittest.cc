// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/host_override.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

TEST(HostOverrideTest, FromString_ValidNoPort) {
  auto override = HostOverride::FromString("localhost.corp.google.com");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("localhost.corp.google.com", override->host);
  EXPECT_FALSE(override->port.has_value());
}

TEST(HostOverrideTest, FromString_ValidWithPort) {
  auto override = HostOverride::FromString("localhost.corp.google.com:8888");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("localhost.corp.google.com", override->host);
  ASSERT_TRUE(override->port.has_value());
  EXPECT_EQ(8888, *override->port);
}

TEST(HostOverrideTest, FromString_ValidWithPortZero) {
  auto override = HostOverride::FromString("localhost.corp.google.com:0");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("localhost.corp.google.com", override->host);
  ASSERT_TRUE(override->port.has_value());
  EXPECT_EQ(0, *override->port);
}

TEST(HostOverrideTest, FromString_IPv6WithPort) {
  auto override = HostOverride::FromString("[::1]:8888");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("::1", override->host);
  ASSERT_TRUE(override->port.has_value());
  EXPECT_EQ(8888, *override->port);
}

TEST(HostOverrideTest, FromString_IPv6NoPort) {
  auto override = HostOverride::FromString("[::1]");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("::1", override->host);
  EXPECT_FALSE(override->port.has_value());
}

TEST(HostOverrideTest, FromString_UnbracketedIPv6) {
  auto override = HostOverride::FromString("::1");
  ASSERT_TRUE(override.has_value());
  EXPECT_EQ("::1", override->host);
  EXPECT_FALSE(override->port.has_value());
}

TEST(HostOverrideTest, FromString_InvalidPort) {
  EXPECT_FALSE(
      HostOverride::FromString("localhost.corp.google.com:99999").has_value());
  EXPECT_FALSE(
      HostOverride::FromString("localhost.corp.google.com:abc").has_value());
  EXPECT_FALSE(
      HostOverride::FromString("localhost.corp.google.com:").has_value());
}

TEST(HostOverrideTest, FromString_Empty) {
  EXPECT_FALSE(HostOverride::FromString("").has_value());
}

TEST(HostOverrideTest, ToString) {
  HostOverride without_port{"localhost.corp.google.com", std::nullopt};
  EXPECT_EQ("localhost.corp.google.com", without_port.ToString());

  HostOverride with_port{"localhost.corp.google.com", 8888};
  EXPECT_EQ("localhost.corp.google.com:8888", with_port.ToString());

  HostOverride ipv6_without_port{"::1", std::nullopt};
  EXPECT_EQ("[::1]", ipv6_without_port.ToString());

  HostOverride ipv6_with_port{"::1", 8888};
  EXPECT_EQ("[::1]:8888", ipv6_with_port.ToString());
}

TEST(HostOverrideTest, Matches) {
  HostOverride without_port{"localhost.corp.google.com", std::nullopt};
  EXPECT_TRUE(without_port.Matches(
      GURL("https://localhost.corp.google.com/search?q=test")));
  EXPECT_FALSE(without_port.Matches(
      GURL("https://localhost.corp.google.com:8888/search?q=test")));
  EXPECT_FALSE(without_port.Matches(GURL("https://example.com/search?q=test")));

  HostOverride with_port{"localhost.corp.google.com", 8888};
  EXPECT_TRUE(with_port.Matches(
      GURL("https://localhost.corp.google.com:8888/search?q=test")));
  EXPECT_FALSE(with_port.Matches(
      GURL("https://localhost.corp.google.com/search?q=test")));
  EXPECT_FALSE(with_port.Matches(
      GURL("https://localhost.corp.google.com:9999/search?q=test")));
  EXPECT_FALSE(
      with_port.Matches(GURL("https://example.com:8888/search?q=test")));

  // Case-insensitivity check.
  EXPECT_TRUE(with_port.Matches(
      GURL("https://LOCALHOST.CORP.GOOGLE.COM:8888/search?q=test")));

  // IPv6 check.
  HostOverride ipv6_with_port{"::1", 8888};
  EXPECT_TRUE(ipv6_with_port.Matches(GURL("https://[::1]:8888/search?q=test")));
  EXPECT_FALSE(
      ipv6_with_port.Matches(GURL("https://[::1]:9999/search?q=test")));
  EXPECT_FALSE(ipv6_with_port.Matches(GURL("https://[::1]/search?q=test")));
}

TEST(HostOverrideTest, ApplyToUrl) {
  HostOverride with_port{"localhost.corp.google.com", 8888};

  // Applies host and port to a standard URL without port.
  GURL url1("https://www.google.com/search?q=test#hash");
  GURL result1 = with_port.ApplyToUrl(url1);
  EXPECT_EQ("https://localhost.corp.google.com:8888/search?q=test#hash",
            result1.spec());
  EXPECT_EQ("localhost.corp.google.com", result1.host());
  EXPECT_EQ(8888, result1.EffectiveIntPort());

  // Overrides an existing host and port.
  GURL url2("https://custom.com:1234/path?param=1");
  GURL result2 = with_port.ApplyToUrl(url2);
  EXPECT_EQ("https://localhost.corp.google.com:8888/path?param=1",
            result2.spec());
  EXPECT_EQ("localhost.corp.google.com", result2.host());
  EXPECT_EQ(8888, result2.EffectiveIntPort());

  // Clears port when override has no port.
  HostOverride without_port{"localhost.corp.google.com", std::nullopt};
  GURL url3("https://custom.com:1234/path?param=1");
  GURL result3 = without_port.ApplyToUrl(url3);
  EXPECT_EQ("https://localhost.corp.google.com/path?param=1", result3.spec());
  EXPECT_EQ("localhost.corp.google.com", result3.host());
  EXPECT_FALSE(result3.has_port());
}

TEST(HostOverrideTest, EqualityOperator) {
  HostOverride a{"host1", 8080};
  HostOverride b{"host1", 8080};
  HostOverride c{"host1", 9090};
  HostOverride d{"host2", 8080};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
}

}  // namespace contextual_tasks
