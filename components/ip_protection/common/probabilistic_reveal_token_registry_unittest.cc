// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_registry.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {

base::Value::Dict CreateRegistryFromJson(const std::string& json_content) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_content, base::JSON_ALLOW_TRAILING_COMMAS);
  CHECK(json.has_value());
  base::Value::Dict* json_dict = json->GetIfDict();
  CHECK(json_dict);
  return std::move(*json_dict);
}

}  // namespace

class ProbabilisticRevealTokenRegistryTest : public testing::Test {
 public:
  ProbabilisticRevealTokenRegistryTest() = default;

 protected:
  ProbabilisticRevealTokenRegistry registry_;
};

TEST_F(ProbabilisticRevealTokenRegistryTest, RegistryTest) {
  // Set the initial registry.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
      "other.com",
    ]
  })json"));

  EXPECT_TRUE(registry_.IsRegistered(GURL("https://example.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("http://example.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://example.com.")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://..example.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://example.com/file.html")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://example.com/some/path")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://foo.example.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://foo.bar.example.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://other.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://OTHER.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://foo.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://example.foo.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://example.com..")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("file:///example.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://example/file.html")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("not-a-url")));
  EXPECT_FALSE(registry_.IsRegistered(GURL()));

  // Update the registry, which should clear the previous entries.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "foo.com",
    ]
  })json"));

  EXPECT_FALSE(registry_.IsRegistered(GURL("https://example.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://other.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://foo.com")));
  EXPECT_TRUE(registry_.IsRegistered(GURL("https://example.foo.com")));

  // IP addresses and eTLDs are not valid entries.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "192.168.0.1",
      "com",
      "co.uk",
    ]
  })json"));
  EXPECT_FALSE(registry_.IsRegistered(GURL("http://192.168.0.1/file.html")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("http://com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("http://.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("http://example.com")));
  EXPECT_FALSE(registry_.IsRegistered(GURL("http://example.co.uk")));

  // Error cases, where the given Dict is not in the expected format. Should
  // retult in an empty registry.

  // Update with an empty object.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({})json"));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://foo.com")));

  // Update where "domains" is not a list.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": {
      "foo.com": true
    }
  })json"));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://foo.com")));

  // Update where the "domains" entries are not strings.
  registry_.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      { "foo.com": true }
    ]
  })json"));
  EXPECT_FALSE(registry_.IsRegistered(GURL("https://foo.com")));
}

TEST_F(ProbabilisticRevealTokenRegistryTest, CustomRegistryTest) {
  // Set the custom registry feature param.
  std::map<std::string, std::string> parameters;
  parameters[net::features::kUseCustomProbabilisticRevealTokenRegistry.name] =
      "true";
  parameters[net::features::kCustomProbabilisticRevealTokenRegistry.name] =
      "test.com,other.com";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableProbabilisticRevealTokens, std::move(parameters));

  // Set the standard registry.
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com"
    ]
  })json"));

  EXPECT_FALSE(registry.IsRegistered(GURL("https://example.com")));
  EXPECT_TRUE(registry.IsRegistered(GURL("https://test.com")));
  EXPECT_TRUE(registry.IsRegistered(GURL("https://foo.test.com")));
  EXPECT_TRUE(registry.IsRegistered(GURL("https://other.com")));
  EXPECT_FALSE(registry.IsRegistered(GURL("https://foo.com")));
}

}  // namespace ip_protection
