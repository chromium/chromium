// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/unindexed_ruleset.h"

#include <memory>
#include <string>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_rule_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

namespace {
namespace proto = url_pattern_index::proto;
namespace testing = url_pattern_index::testing;
using url_pattern_index::UrlPattern;

template <typename T>
bool IsEqual(const T& lhs, const T& rhs) {
  return lhs.SerializeAsString() == rhs.SerializeAsString();
}

// The helper class used for building UnindexedRulesets.
class UnindexedRulesetTestBuilder {
 public:
  // Initializes the builder that writes the ruleset to StringOutputStream.
  UnindexedRulesetTestBuilder()
      : output_(
            new google::protobuf::io::StringOutputStream(&ruleset_contents_)),
        ruleset_writer_(output_.get()) {}

  // Initializes the builder that writes the ruleset to an array of |array_size|
  // through an ArrayOutputStream.
  UnindexedRulesetTestBuilder(int array_size)
      : ruleset_contents_(array_size, '\0'),
        output_(
            new google::protobuf::io::ArrayOutputStream(&ruleset_contents_[0],
                                                        array_size)),
        ruleset_writer_(output_.get()) {}

  UnindexedRulesetTestBuilder(const UnindexedRulesetTestBuilder&) = delete;
  UnindexedRulesetTestBuilder& operator=(const UnindexedRulesetTestBuilder&) =
      delete;

  int max_rules_per_chunk() const {
    return ruleset_writer_.max_rules_per_chunk();
  }

  bool AddUrlRule(const UrlPattern& url_pattern,
                  proto::SourceType source_type,
                  bool is_allowlist = false) {
    auto& rule = url_rules_.emplace_back(testing::MakeUrlRule(url_pattern));
    if (is_allowlist) {
      rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
    }
    rule.set_source_type(source_type);

    return !ruleset_writer_.had_error() && ruleset_writer_.AddUrlRule(rule);
  }

  bool AddUrlRules(int number_of_rules) {
    for (int i = 0; i < number_of_rules; ++i) {
      std::string url_pattern = "example" + base::NumberToString(i) + ".com";
      if (!AddUrlRule(UrlPattern(url_pattern), testing::kAnyParty, i & 1)) {
        return false;
      }
    }
    return true;
  }

  bool AddStyleRule(const std::string& selector,
                    const std::vector<std::string>& domains = {},
                    bool is_allowlist = false) {
    auto& rule = style_rules_.emplace_back();
    rule.set_semantics(is_allowlist ? proto::RULE_SEMANTICS_ALLOWLIST
                                    : proto::RULE_SEMANTICS_BLOCKLIST);
    rule.set_selector(selector);
    for (const auto& domain : domains) {
      auto* item = rule.add_domains();
      if (!domain.empty() && domain[0] == '~') {
        item->set_domain(domain.substr(1));
        item->set_exclude(true);
      } else {
        item->set_domain(domain);
      }
    }

    // Simulate what the real tool does by populating pre-parsed fields.
    if (selector == "#id") {
      rule.add_ids("id");
    } else if (selector == ".class") {
      rule.add_classes("class");
    }

    return !ruleset_writer_.had_error() && ruleset_writer_.AddStyleRule(rule);
  }

  bool Finish() {
    if (!ruleset_writer_.had_error() && ruleset_writer_.Finish()) {
      // Note: This line has effect only when |output_| is an ArrayOutputStream.
      ruleset_contents_.resize(output_->ByteCount());
      return true;
    }
    return false;
  }

  const std::vector<proto::UrlRule>& url_rules() const { return url_rules_; }
  const std::vector<proto::StyleRule>& style_rules() const {
    return style_rules_;
  }
  const std::string& ruleset_contents() const { return ruleset_contents_; }

 private:
  std::vector<proto::UrlRule> url_rules_;
  std::vector<proto::StyleRule> style_rules_;
  std::string ruleset_contents_;
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> output_;
  UnindexedRulesetWriter ruleset_writer_;
};

bool IsRulesetValid(
    const std::string& ruleset_contents,
    const std::vector<proto::UrlRule>& expected_url_rules,
    const std::vector<proto::StyleRule>& expected_style_rules = {}) {
  google::protobuf::io::ArrayInputStream array_input(ruleset_contents.data(),
                                                     ruleset_contents.size());
  UnindexedRulesetReader reader(&array_input);
  proto::FilteringRules chunk;
  std::vector<proto::UrlRule> read_url_rules;
  std::vector<proto::StyleRule> read_style_rules;
  while (reader.ReadNextChunk(&chunk)) {
    read_url_rules.insert(read_url_rules.end(), chunk.url_rules().begin(),
                          chunk.url_rules().end());
    read_style_rules.insert(read_style_rules.end(), chunk.style_rules().begin(),
                            chunk.style_rules().end());
  }
  if (base::checked_cast<size_t>(reader.num_bytes_read()) !=
      ruleset_contents.size()) {
    return false;
  }

  if (expected_url_rules.size() != read_url_rules.size()) {
    return false;
  }
  for (size_t i = 0, size = read_url_rules.size(); i != size; ++i) {
    if (!IsEqual(expected_url_rules[i], read_url_rules[i])) {
      return false;
    }
  }

  if (expected_style_rules.size() != read_style_rules.size()) {
    return false;
  }
  for (size_t i = 0, size = read_style_rules.size(); i != size; ++i) {
    if (!IsEqual(expected_style_rules[i], read_style_rules[i])) {
      return false;
    }
  }

  return true;
}

}  // namespace

TEST(UnindexedRulesetTest, EmptyRuleset) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, OneUrlRule) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(
      builder.AddUrlRule(UrlPattern("example.com"), testing::kThirdParty));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, OneStyleRule) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddStyleRule("#id", {"example.com"}));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules(),
                             builder.style_rules()));
}

TEST(UnindexedRulesetTest, PreParsedStyleRule) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddStyleRule(".class"));
  EXPECT_TRUE(builder.Finish());

  // Verify that the pre-parsed anchor was actually stored.
  google::protobuf::io::ArrayInputStream array_input(
      builder.ruleset_contents().data(), builder.ruleset_contents().size());
  UnindexedRulesetReader reader(&array_input);
  proto::FilteringRules chunk;
  ASSERT_TRUE(reader.ReadNextChunk(&chunk));
  ASSERT_EQ(1, chunk.style_rules_size());
  EXPECT_EQ(1, chunk.style_rules(0).classes_size());
  EXPECT_EQ("class", chunk.style_rules(0).classes(0));
}

TEST(UnindexedRulesetTest, ManyUrlRules) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(1234));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, ExactlyMaxRulesPerChunk) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(builder.max_rules_per_chunk()));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, MaxRulesPerChunkPlusOne) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(builder.max_rules_per_chunk() + 1));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, ErrorOnWrite) {
  UnindexedRulesetTestBuilder builder(1000);
  EXPECT_FALSE(builder.AddUrlRules(1234));
}

TEST(UnindexedRulesetTest, ReadCorruptedInput) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(1000));
  EXPECT_TRUE(builder.Finish());

  {
    std::string ruleset_contents = builder.ruleset_contents();
    ASSERT_GE(ruleset_contents.size(), static_cast<size_t>(2000));
    ruleset_contents[100] ^= 239;
    ruleset_contents[1000] ^= 3;
    EXPECT_FALSE(IsRulesetValid(ruleset_contents, builder.url_rules()));
  }

  {
    std::string ruleset_contents = builder.ruleset_contents();
    ASSERT_GT(ruleset_contents.size(), static_cast<size_t>(100));
    ruleset_contents.resize(100);
    EXPECT_FALSE(IsRulesetValid(ruleset_contents, builder.url_rules()));
  }
}

}  // namespace subresource_filter
