// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_INDEXER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_INDEXER_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/subresource_filter/core/common/flat/style_rule_index_generated.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace subresource_filter {

// The class used to construct flat data structures representing the index of
// style rules.
class StyleRuleIndexer {
 public:
  // The style rule index used for implicit selectors. Implicit selectors
  // aren't explicitly listed in the flatbuffer because the class name or id is
  // sufficient for generating the entire selector.
  static constexpr uint16_t kImplicitStyleRuleIndex = 65535;

  explicit StyleRuleIndexer(flatbuffers::FlatBufferBuilder* builder);

  StyleRuleIndexer(const StyleRuleIndexer&) = delete;
  StyleRuleIndexer& operator=(const StyleRuleIndexer&) = delete;

  ~StyleRuleIndexer();

  // Adds a style rule to the index from a proto. Returns whether the |rule| has
  // been serialized and added to the index.
  bool AddStyleRuleFromProto(const url_pattern_index::proto::StyleRule& rule);

  // Finalizes construction of the style rule index in the |builder|.
  // Returns the offset of the serialized flat::StyleRuleIndex.
  flatbuffers::Offset<flat::StyleRuleIndex> Finish();

 private:
  using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;

  std::optional<uint16_t> GetOrCreateStyleRuleIndex(
      const std::string& selector,
      bool is_site_specific,
      bool is_exclusion,
      const google::protobuf::RepeatedPtrField<std::string>& classes,
      const google::protobuf::RepeatedPtrField<std::string>& ids);

  std::optional<uint16_t> CreateExplicitStyleRule(const std::string& selector);

  bool IndexExclusionRule(
      uint16_t rule_index,
      const google::protobuf::RepeatedPtrField<
          url_pattern_index::proto::DomainListItem>& domains);

  bool IndexHidingRule(
      uint16_t rule_index,
      const google::protobuf::RepeatedPtrField<
          url_pattern_index::proto::DomainListItem>& domains,
      const google::protobuf::RepeatedPtrField<std::string>& classes,
      const google::protobuf::RepeatedPtrField<std::string>& ids);

  void AddToIndexMap(std::map<std::string, std::vector<uint16_t>>& map,
                     std::string_view key,
                     uint16_t val);

  std::vector<FlatStringOffset> style_rules_;

  // std::map used below to ensure sorted order.
  std::map<std::string, std::vector<uint16_t>> domain_map_;
  std::map<std::string, std::vector<uint16_t>> exclusion_map_;
  std::map<std::string, std::vector<uint16_t>> class_map_;
  std::map<std::string, std::vector<uint16_t>> id_map_;

  std::map<std::string, uint16_t> selector_to_index_;

  raw_ptr<flatbuffers::FlatBufferBuilder> builder_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_INDEXER_H_
