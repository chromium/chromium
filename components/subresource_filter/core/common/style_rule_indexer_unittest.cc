// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_indexer.h"

#include "components/subresource_filter/core/common/flat/style_rule_index_generated.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace subresource_filter {

TEST(StyleRuleIndexerTest, Basic) {
  flatbuffers::FlatBufferBuilder builder;
  StyleRuleIndexer indexer(&builder);

  url_pattern_index::proto::StyleRule rule;
  rule.set_selector("#ad");
  rule.set_semantics(url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST);

  EXPECT_TRUE(indexer.AddStyleRuleFromProto(rule));

  auto offset = indexer.Finish();
  EXPECT_FALSE(offset.IsNull());
}

TEST(StyleRuleIndexerTest, EmptySelector) {
  flatbuffers::FlatBufferBuilder builder;
  StyleRuleIndexer indexer(&builder);

  url_pattern_index::proto::StyleRule rule;
  EXPECT_FALSE(indexer.AddStyleRuleFromProto(rule));
}

TEST(StyleRuleIndexerTest, GlobalExclusion) {
  flatbuffers::FlatBufferBuilder builder;
  StyleRuleIndexer indexer(&builder);

  url_pattern_index::proto::StyleRule rule;
  rule.set_selector(".ad");
  rule.set_semantics(url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST);

  EXPECT_FALSE(indexer.AddStyleRuleFromProto(rule));

  auto offset = indexer.Finish();
  builder.Finish(offset);
  auto* index =
      flatbuffers::GetRoot<flat::StyleRuleIndex>(builder.GetBufferPointer());

  ASSERT_TRUE(index->selectors());
  EXPECT_EQ(0u, index->selectors()->size());
}

TEST(StyleRuleIndexerTest, ExcludeDomain) {
  flatbuffers::FlatBufferBuilder builder;
  StyleRuleIndexer indexer(&builder);

  url_pattern_index::proto::StyleRule rule;
  rule.set_selector(".ad");
  rule.set_semantics(url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST);
  auto* item = rule.add_domains();
  item->set_domain("example.com");
  item->set_exclude(true);

  EXPECT_FALSE(indexer.AddStyleRuleFromProto(rule));

  auto offset = indexer.Finish();
  builder.Finish(offset);
  auto* index =
      flatbuffers::GetRoot<flat::StyleRuleIndex>(builder.GetBufferPointer());

  ASSERT_TRUE(index->selectors());
  // The selector is not allocated because we fail early in
  // AddStyleRuleFromProto.
  EXPECT_EQ(0u, index->selectors()->size());
  EXPECT_EQ(0u, index->domain_map()->size());
}

TEST(StyleRuleIndexerTest, SelectorSharing) {
  flatbuffers::FlatBufferBuilder builder;
  StyleRuleIndexer indexer(&builder);

  // 1. Two site-specific rules with the same selector.
  EXPECT_TRUE(indexer.AddStyleRuleFromProto(
      testing::CreateStyleRule(".shared", {"a.com"}, false, {"shared"})));
  EXPECT_TRUE(indexer.AddStyleRuleFromProto(
      testing::CreateStyleRule(".shared", {"b.com"}, false, {"shared"})));

  // 2. Generic rule
  EXPECT_TRUE(indexer.AddStyleRuleFromProto(
      testing::CreateStyleRule(".shared", {}, false, {"shared"})));

  auto offset = indexer.Finish();
  builder.Finish(offset);

  auto* index =
      flatbuffers::GetRoot<flat::StyleRuleIndex>(builder.GetBufferPointer());

  // They should share the same selector string in the `selectors` vector.
  ASSERT_TRUE(index->selectors());
  EXPECT_EQ(1u, index->selectors()->size()) << "Selectors should be shared";
}

}  // namespace subresource_filter
