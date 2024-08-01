// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/subresource_filter/core/common/test_ruleset_creator.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

// The methods below assume that char and uint8_t are interchangeable.
static_assert(CHAR_BIT == 8, "Assumed char was 8 bits.");

void WriteRulesetContents(const std::vector<uint8_t>& contents,
                          base::FilePath path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::WriteFile(path, contents));
}

std::vector<uint8_t> SerializeUnindexedRulesetWithMultipleRules(
    const std::vector<proto::UrlRule>& rules) {
  std::string ruleset_contents;
  google::protobuf::io::StringOutputStream output(&ruleset_contents);
  UnindexedRulesetWriter ruleset_writer(&output);
  for (const auto& rule : rules)
    ruleset_writer.AddUrlRule(rule);
  ruleset_writer.Finish();

  auto* data = reinterpret_cast<const uint8_t*>(ruleset_contents.data());
  return std::vector<uint8_t>(data, data + ruleset_contents.size());
}

std::vector<uint8_t> SerializeIndexedRulesetWithMultipleRules(
    const std::vector<proto::UrlRule>& rules) {
  RulesetIndexer indexer;
  for (const auto& rule : rules)
    EXPECT_TRUE(indexer.AddUrlRule(rule));
  indexer.Finish();
  return std::vector<uint8_t>(indexer.data().begin(), indexer.data().end());
}

}  // namespace

namespace testing {

// TestRuleset -----------------------------------------------------------------

TestRuleset::TestRuleset() = default;
TestRuleset::~TestRuleset() = default;

// static
base::File TestRuleset::Open(const TestRuleset& ruleset) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file;
  file.Initialize(ruleset.path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_SHARE_DELETE);
  return file;
}

// static
void TestRuleset::CorruptByTruncating(const TestRuleset& ruleset,
                                      size_t tail_size) {
  ASSERT_GT(tail_size, 0u);
  ASSERT_FALSE(ruleset.contents.empty());
  std::vector<uint8_t> new_contents = ruleset.contents;

  const size_t new_size =
      tail_size < new_contents.size() ? new_contents.size() - tail_size : 0;
  new_contents.resize(new_size);
  WriteRulesetContents(new_contents, ruleset.path);
}

// static
void TestRuleset::CorruptByFilling(const TestRuleset& ruleset,
                                   size_t from,
                                   size_t to,
                                   uint8_t fill_with) {
  ASSERT_LT(from, to);
  ASSERT_LE(to, ruleset.contents.size());

  std::vector<uint8_t> new_contents = ruleset.contents;
  for (size_t i = from; i < to; ++i)
    new_contents[i] = fill_with;
  WriteRulesetContents(new_contents, ruleset.path);
}

// TestRulesetPair -------------------------------------------------------------

TestRulesetPair::TestRulesetPair() = default;
TestRulesetPair::~TestRulesetPair() = default;

// TestRulesetCreator ----------------------------------------------------------

TestRulesetCreator::TestRulesetCreator()
    : scoped_temp_dir_(std::make_unique<base::ScopedTempDir>()) {}

TestRulesetCreator::~TestRulesetCreator() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  scoped_temp_dir_.reset();
}

void TestRulesetCreator::CreateRulesetToDisallowURLsWithPathSuffix(
    std::string_view suffix,
    TestRulesetPair* test_ruleset_pair) {
  CHECK(test_ruleset_pair);
  proto::UrlRule suffix_rule = CreateSuffixRule(suffix);
  CreateRulesetWithRules({suffix_rule}, test_ruleset_pair);
}

void TestRulesetCreator::CreateUnindexedRulesetToDisallowURLsWithPathSuffix(
    std::string_view suffix,
    TestRuleset* test_unindexed_ruleset) {
  CHECK(test_unindexed_ruleset);
  proto::UrlRule suffix_rule = CreateSuffixRule(suffix);
  ASSERT_NO_FATAL_FAILURE(
      CreateUnindexedRulesetWithRules({suffix_rule}, test_unindexed_ruleset));
}

void TestRulesetCreator::CreateRulesetToDisallowURLWithSubstrings(
    std::vector<std::string_view> substrings,
    TestRulesetPair* test_ruleset_pair) {
  CHECK(test_ruleset_pair);
  std::vector<proto::UrlRule> url_rules;
  for (const auto& substring : substrings)
    url_rules.push_back(CreateSubstringRule(substring));
  CreateRulesetWithRules(url_rules, test_ruleset_pair);
}

void TestRulesetCreator::CreateRulesetToDisallowURLsWithManySuffixes(
    std::string_view suffix,
    int num_of_suffixes,
    TestRulesetPair* test_ruleset_pair) {
  CHECK(test_ruleset_pair);

  std::vector<proto::UrlRule> rules;
  for (int i = 0; i < num_of_suffixes; ++i) {
    std::string current_suffix =
        std::string(suffix) + '_' + base::NumberToString(i);
    rules.push_back(CreateSuffixRule(current_suffix));
  }
  CreateRulesetWithRules(rules, test_ruleset_pair);
}

void TestRulesetCreator::CreateRulesetWithRules(
    const std::vector<proto::UrlRule>& rules,
    TestRulesetPair* test_ruleset_pair) {
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeUnindexedRulesetWithMultipleRules(rules),
      &test_ruleset_pair->unindexed));
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeIndexedRulesetWithMultipleRules(rules),
      &test_ruleset_pair->indexed));
}

void TestRulesetCreator::CreateUnindexedRulesetWithRules(
    const std::vector<proto::UrlRule>& rules,
    TestRuleset* test_unindexed_ruleset) {
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeUnindexedRulesetWithMultipleRules(rules),
      test_unindexed_ruleset));
}

void TestRulesetCreator::GetUniqueTemporaryPath(base::FilePath* path) {
  CHECK(path);
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(scoped_temp_dir_->IsValid() ||
              scoped_temp_dir_->CreateUniqueTempDir());
  *path = scoped_temp_dir_->GetPath().AppendASCII(
      base::NumberToString(next_unique_file_suffix++));
}

void TestRulesetCreator::CreateTestRulesetFromContents(
    std::vector<uint8_t> ruleset_contents,
    TestRuleset* ruleset) {
  CHECK(ruleset);

  ruleset->contents = std::move(ruleset_contents);
  ASSERT_NO_FATAL_FAILURE(GetUniqueTemporaryPath(&ruleset->path));
  WriteRulesetContents(ruleset->contents, ruleset->path);
}

}  // namespace testing
}  // namespace subresource_filter
