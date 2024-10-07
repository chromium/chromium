// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/filter_tool.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

std::string CreateJsonLine(const std::string& origin,
                           const std::string& request_url,
                           const std::string& request_type) {
  base::Value::Dict dictionary;
  dictionary.Set("origin", origin);
  dictionary.Set("request_url", request_url);
  dictionary.Set("request_type", request_type);

  std::string output;
  EXPECT_TRUE(base::JSONWriter::Write(dictionary, &output));
  return output + "\n";
}

class FilterToolTest : public ::testing::Test {
 public:
  FilterToolTest() = default;

  FilterToolTest(const FilterToolTest&) = delete;
  FilterToolTest& operator=(const FilterToolTest&) = delete;

 protected:
  void SetUp() override {
    CreateRuleset();
    filter_tool_ = std::make_unique<FilterTool>(ruleset_, &out_stream_);
  }

  void CreateRuleset() {
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateSuffixRule("disallowed1.png"));
    rules.push_back(testing::CreateSuffixRule("disallowed2.png"));
    rules.push_back(testing::CreateSuffixRule("disallowed3.png"));
    rules.push_back(
        testing::CreateAllowlistSuffixRule("allowlist/disallowed1.png"));
    rules.push_back(
        testing::CreateAllowlistSuffixRule("allowlist/disallowed2.png"));

    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    ruleset_ = MemoryMappedRuleset::CreateAndInitialize(
        testing::TestRuleset::Open(test_ruleset_pair_.indexed));
  }

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  std::ostringstream out_stream_;
  std::unique_ptr<FilterTool> filter_tool_;
};

TEST_F(FilterToolTest, MatchBlocklist) {
  filter_tool_->Match("http://example.com",
                      "http://example.com/disallowed1.png", "image");

  std::string expected =
      "BLOCKED disallowed1.png| http://example.com "
      "http://example.com/disallowed1.png "
      "image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchAllowlist) {
  filter_tool_->Match("http://example.com",
                      "http://example.com/allowlist/disallowed1.png", "image");
  std::string expected =
      "ALLOWED @@allowlist/disallowed1.png| http://example.com "
      "http://example.com/allowlist/disallowed1.png "
      "image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, NoMatch) {
  filter_tool_->Match("http://example.com", "http://example.com/noproblem.png",
                      "image");

  std::string expected =
      "ALLOWED http://example.com http://example.com/noproblem.png image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchBatch) {
  std::stringstream batch_queries;
  batch_queries << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed2.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image");

  filter_tool_->MatchBatch(&batch_queries);

  std::string expected =
      "BLOCKED disallowed1.png| http://example.com "
      "http://example.com/disallowed1.png image\n"
      "BLOCKED disallowed2.png| http://example.com "
      "http://example.com/disallowed2.png image\n"
      "ALLOWED @@allowlist/disallowed2.png| http://example.com "
      "http://example.com/allowlist/disallowed2.png image\n";

  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchRules) {
  std::stringstream batch_queries;
  batch_queries << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed2.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image");

  filter_tool_->MatchRules(&batch_queries, 1);

  std::string result = out_stream_.str();

  std::string expected =
      "3 disallowed1.png|\n"
      "2 @@allowlist/disallowed2.png|\n"
      "1 disallowed2.png|\n";

  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchRulesMinCount) {
  std::stringstream batch_queries;
  batch_queries << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed1.png", "image")
                << CreateJsonLine("http://example.com",
                                  "http://example.com/disallowed2.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image")
                << CreateJsonLine(
                       "http://example.com",
                       "http://example.com/allowlist/disallowed2.png", "image");

  filter_tool_->MatchRules(&batch_queries, 2);

  std::string expected =
      "3 @@allowlist/disallowed2.png|\n"
      "2 disallowed1.png|\n";

  EXPECT_EQ(expected, out_stream_.str());
}

}  // namespace

}  // namespace subresource_filter
