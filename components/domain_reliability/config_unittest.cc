// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/config.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

std::unique_ptr<DomainReliabilityConfig> MakeBaseConfig() {
  DomainReliabilityConfig* config = new DomainReliabilityConfig();
  config->origin = url::Origin::Create(GURL("https://example/"));
  config->include_subdomains = false;
  config->collectors.push_back(
      std::make_unique<GURL>("https://example/upload"));
  config->failure_sample_rate = 1.0;
  config->success_sample_rate = 0.0;
  EXPECT_TRUE(config->IsValid());
  return std::unique_ptr<DomainReliabilityConfig>(config);
}

std::unique_ptr<DomainReliabilityConfig> MakeSampleConfig() {
  std::unique_ptr<DomainReliabilityConfig> config(MakeBaseConfig());
  config->path_prefixes.push_back(std::make_unique<std::string>("/css/"));
  config->path_prefixes.push_back(std::make_unique<std::string>("/js/"));
  EXPECT_TRUE(config->IsValid());
  return config;
}

class DomainReliabilityConfigTest : public testing::Test { };

TEST_F(DomainReliabilityConfigTest, IsValid) {
  std::unique_ptr<DomainReliabilityConfig> config;

  config = MakeSampleConfig();
  EXPECT_TRUE(config->IsValid());

  config = MakeSampleConfig();
  config->origin = url::Origin();
  EXPECT_FALSE(config->IsValid());

  config = MakeSampleConfig();
  config->collectors.clear();
  EXPECT_FALSE(config->IsValid());

  config = MakeSampleConfig();
  config->collectors[0] = std::make_unique<GURL>();
  EXPECT_FALSE(config->IsValid());

  config = MakeSampleConfig();
  config->failure_sample_rate = 2.0;
  EXPECT_FALSE(config->IsValid());

  config = MakeSampleConfig();
  config->success_sample_rate = 2.0;
  EXPECT_FALSE(config->IsValid());
}

TEST_F(DomainReliabilityConfigTest, FromJSON) {
  std::string config_json =
    "{ \"origin\": \"https://example/\","
    "  \"include_subdomains\": false,"
    "  \"collectors\": [ \"https://example/upload\" ],"
    "  \"path_prefixes\": ["
    "    \"/css/\","
    "    \"/js/\""
    "  ],"
    "  \"failure_sample_rate\": 0.10,"
    "  \"success_sample_rate\": 0.01"
    "}";

  std::unique_ptr<const DomainReliabilityConfig> config(
      DomainReliabilityConfig::FromJSON(config_json));

  EXPECT_TRUE(config);
  EXPECT_EQ("https://example", config->origin.Serialize());
  EXPECT_FALSE(config->include_subdomains);
  EXPECT_EQ(1u, config->collectors.size());
  EXPECT_EQ(GURL("https://example/upload"), *config->collectors[0]);
  EXPECT_EQ(2u, config->path_prefixes.size());
  EXPECT_EQ("/css/", *config->path_prefixes[0]);
  EXPECT_EQ("/js/", *config->path_prefixes[1]);
  EXPECT_EQ(0.10, config->failure_sample_rate);
  EXPECT_EQ(0.01, config->success_sample_rate);
}

}  // namespace
}  // namespace domain_reliability
