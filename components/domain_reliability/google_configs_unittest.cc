// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/google_configs.h"

#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

// This really only checks domains, not origins.
bool HasSameOriginCollector(const DomainReliabilityConfig* config) {
  for (const auto& collector : config->collectors) {
    if (collector->host() == config->origin.host())
      return true;
  }
  return false;
}

TEST(DomainReliabilityGoogleConfigsTest, ConfigsAreValid) {
  auto configs = GetAllGoogleConfigsForTesting();
  for (const auto& config : configs) {
    EXPECT_TRUE(config->IsValid());
  }
}

TEST(DomainReliabilityGoogleConfigsTest, MaybeGetGoogleConfig) {
  // Includes subdomains and includes same-origin collector.
  std::string host = "google.ac";
  auto config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
  EXPECT_TRUE(config->include_subdomains);
  EXPECT_TRUE(HasSameOriginCollector(config.get()));

  // Includes subdomains and excludes same-origin collector.
  host = "2mdn.net";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
  EXPECT_TRUE(config->include_subdomains);
  EXPECT_FALSE(HasSameOriginCollector(config.get()));

  // Excludes subdomains and includes same-origin collector.
  host = "accounts.google.com";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
  EXPECT_FALSE(config->include_subdomains);
  EXPECT_TRUE(HasSameOriginCollector(config.get()));

  // Excludes subdomains and excludes same-origin collector.
  host = "ad.doubleclick.net";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
  EXPECT_FALSE(config->include_subdomains);
  EXPECT_FALSE(HasSameOriginCollector(config.get()));
}

TEST(DomainReliabilityGoogleConfigsTest, MaybeGetGoogleConfigSubdomains) {
  // google.ac is duplicated for the www. subdomain so it generates a
  // subdomain-specific config.
  std::string host = "www.google.ac";
  auto config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
  // Subdomains are not included for the www version.
  EXPECT_FALSE(config->include_subdomains);

  // Other subdomains match the parent config.
  host = "subdomain.google.ac";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(net::GetSuperdomain(host), config->origin.host());
  EXPECT_TRUE(config->include_subdomains);

  // 2mdn.net is not duplicated for www, but it includes subdomains, so
  // www.2mdn.net is covered.
  host = "www.2mdn.net";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(net::GetSuperdomain(host), config->origin.host());
  EXPECT_TRUE(config->include_subdomains);

  // drive.google.com does not include subdomains and is not duplicated for www.
  host = "subdomain.drive.google.com";
  config = MaybeGetGoogleConfig(host);
  EXPECT_FALSE(config);
  host = "www.drive.google.com";
  config = MaybeGetGoogleConfig(host);
  EXPECT_FALSE(config);

  // accounts.google.com should get its own config, even though it is a
  // subdomain of google.com (which does include subdomains), because an exact
  // match takes priority.
  host = "accounts.google.com";
  config = MaybeGetGoogleConfig(host);
  EXPECT_EQ(host, config->origin.host());
}

}  // namespace

}  // namespace domain_reliability
