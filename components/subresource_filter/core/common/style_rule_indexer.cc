// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_indexer.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "third_party/rapidhash/rapidhash.h"

namespace subresource_filter {

namespace {

// Helper function to set bits in Bloom filter.
// The bloom filter makes 2 hashes out of the 24 bit blink string hash. The
// first is the first 16 bits. The second shuffles the original 24 bit hash
// and then takes 16 bits from that. This is better than just using the first
// 16 bits and the final 8 bits as keys.
void SetBloomBits(std::vector<uint8_t>& filter, uint32_t hash) {
  uint32_t num_bits = filter.size() * 8;
  size_t bit_index1 = hash % num_bits;
  size_t bit_index2 =
      static_cast<size_t>((static_cast<uint64_t>(hash) * 16777619u) % num_bits);
  filter[bit_index1 / 8] |= (1 << (bit_index1 % 8));
  filter[bit_index2 / 8] |= (1 << (bit_index2 % 8));
}

// Helper function to write a map of names->selector indices into `builder`.
// Returns the offset into the flatbuffer.
flatbuffers::Offset<
    flatbuffers::Vector<flatbuffers::Offset<flat::NameToSelectors>>>
BuildNameToRules(
    flatbuffers::FlatBufferBuilder& builder,
    const std::map<std::string, std::vector<uint16_t>>& rules_map) {
  std::vector<flatbuffers::Offset<flat::NameToSelectors>> offsets;

  for (const auto& [name, indices] : rules_map) {
    auto name_offset = builder.CreateSharedString(name);
    uint16_t first_index = StyleRuleIndexer::kImplicitStyleRuleIndex;
    flatbuffers::Offset<flatbuffers::Vector<uint16_t>> more_offset;

    if (!indices.empty()) {
      // Ensure that if the implicit rule is in the list, that it goes to
      // `first_index`.
      bool has_implicit = std::ranges::contains(
          indices, StyleRuleIndexer::kImplicitStyleRuleIndex);
      first_index =
          has_implicit ? StyleRuleIndexer::kImplicitStyleRuleIndex : indices[0];
      std::vector<uint16_t> more;
      more.reserve(indices.size() - 1);
      std::ranges::copy_if(
          indices.begin(), indices.end(), std::back_inserter(more),
          [first_index](int val) { return val != first_index; });

      if (!more.empty()) {
        more_offset = builder.CreateVector(more);
      }
    }
    offsets.emplace_back(flat::CreateNameToSelectors(builder, name_offset,
                                                     first_index, more_offset));
  }
  return builder.CreateVector(offsets);
}

}  // namespace

StyleRuleIndexer::StyleRuleIndexer(flatbuffers::FlatBufferBuilder* builder)
    : builder_(builder) {}

StyleRuleIndexer::~StyleRuleIndexer() = default;

bool StyleRuleIndexer::AddStyleRuleFromProto(
    const url_pattern_index::proto::StyleRule& rule) {
  // We reject empty rules.
  if (!rule.has_selector() || rule.selector().empty()) {
    return false;
  }

  bool is_exclusion =
      rule.semantics() == url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST;

  // Global exclusions are not supported.
  if (is_exclusion && rule.domains().empty()) {
    return false;
  }

  // Exclusions in domains are not supported.
  for (const auto& domain_item : rule.domains()) {
    if (domain_item.exclude()) {
      return false;
    }
  }

  std::optional<uint16_t> rule_index = GetOrCreateStyleRuleIndex(
      rule.selector(), /*is_site_specific=*/!rule.domains().empty(),
      /*is_exclusion=*/is_exclusion, rule.classes(), rule.ids());

  if (!rule_index) {
    return false;
  }

  if (is_exclusion) {
    return IndexExclusionRule(*rule_index, rule.domains());
  }
  return IndexHidingRule(*rule_index, rule.domains(), rule.classes(),
                         rule.ids());
}

std::optional<uint16_t> StyleRuleIndexer::GetOrCreateStyleRuleIndex(
    const std::string& selector,
    bool is_site_specific,
    bool is_exclusion,
    const google::protobuf::RepeatedPtrField<std::string>& classes,
    const google::protobuf::RepeatedPtrField<std::string>& ids) {
  std::map<std::string, uint16_t>::iterator it =
      selector_to_index_.find(selector);
  if (it != selector_to_index_.end()) {
    // We have an existing index for this selector.
    uint16_t rule_index = it->second;

    // The incoming unindexed ruleset should be in order of site-specific
    // before global rules. If it's not, we DCHECK instead of CHECK to avoid
    // a crash, as in the worst case a exception rule may fail to properly
    // apply, but that is better than crashing the browser.
    DCHECK(!(rule_index == kImplicitStyleRuleIndex && is_site_specific))
        << "Site-specific rule processed after global rule for: " << selector;
    return rule_index;
  }

  // An implicit index doesn't index into the selectors, it means "just use
  // the class or id from the lookup map as the selector". These can only be
  // used on global rules where the selectors exactly matches the class or id.
  std::string_view selector_view(selector);
  bool can_be_implicit =
      !is_site_specific && !is_exclusion &&
      ((classes.size() == 1 && ids.empty() && selector_view.starts_with('.') &&
        selector_view.substr(1) == classes.at(0)) ||
       (ids.size() == 1 && classes.empty() && selector_view.starts_with('#') &&
        selector_view.substr(1) == ids.at(0)));

  if (can_be_implicit) {
    selector_to_index_[selector] = kImplicitStyleRuleIndex;
    return kImplicitStyleRuleIndex;
  }

  auto explicit_index = CreateExplicitStyleRule(selector);
  if (!explicit_index) {
    return std::nullopt;
  }
  selector_to_index_[selector] = *explicit_index;
  return explicit_index;
}

std::optional<uint16_t> StyleRuleIndexer::CreateExplicitStyleRule(
    const std::string& selector) {
  if (style_rules_.size() >= kImplicitStyleRuleIndex) {
    LOG(ERROR) << "Maximum number of explicit style rules reached ("
               << kImplicitStyleRuleIndex
               << "). Skipping rule with selector: " << selector;
    return std::nullopt;
  }
  auto selector_offset = builder_->CreateSharedString(selector);
  uint16_t rule_index = base::checked_cast<uint16_t>(style_rules_.size());
  style_rules_.emplace_back(selector_offset);
  return rule_index;
}

bool StyleRuleIndexer::IndexExclusionRule(
    uint16_t rule_index,
    const google::protobuf::RepeatedPtrField<
        url_pattern_index::proto::DomainListItem>& domains) {
  for (const url_pattern_index::proto::DomainListItem& domain_item : domains) {
    if (domain_item.domain().empty()) {
      continue;
    }
    DCHECK(!domain_item.exclude());
    AddToIndexMap(exclusion_map_, domain_item.domain(), rule_index);
  }
  return true;
}

bool StyleRuleIndexer::IndexHidingRule(
    uint16_t rule_index,
    const google::protobuf::RepeatedPtrField<
        url_pattern_index::proto::DomainListItem>& domains,
    const google::protobuf::RepeatedPtrField<std::string>& classes,
    const google::protobuf::RepeatedPtrField<std::string>& ids) {
  bool is_site_specific = false;
  for (const url_pattern_index::proto::DomainListItem& domain_item : domains) {
    if (domain_item.domain().empty()) {
      continue;
    }
    DCHECK(!domain_item.exclude());
    is_site_specific = true;
    AddToIndexMap(domain_map_, domain_item.domain(), rule_index);
  }

  if (!is_site_specific) {
    for (const std::string& class_name : classes) {
      AddToIndexMap(class_map_, "." + class_name, rule_index);
    }
    for (const std::string& id_name : ids) {
      AddToIndexMap(id_map_, "#" + id_name, rule_index);
    }
  }
  return true;
}

void StyleRuleIndexer::AddToIndexMap(
    std::map<std::string, std::vector<uint16_t>>& map,
    std::string_view key,
    uint16_t val) {
  std::vector<uint16_t>& vec = map[std::string(key)];
  if (vec.empty() || vec.back() != val) {
    vec.emplace_back(val);
  }
}

flatbuffers::Offset<flat::StyleRuleIndex> StyleRuleIndexer::Finish() {
  auto selectors_offset = builder_->CreateVector(style_rules_);

  auto domain_map_offset = BuildNameToRules(*builder_, domain_map_);
  auto exclusion_map_offset = BuildNameToRules(*builder_, exclusion_map_);
  auto class_map_offset = BuildNameToRules(*builder_, class_map_);
  auto id_map_offset = BuildNameToRules(*builder_, id_map_);

  size_t expected_rule_count = class_map_.size() + id_map_.size();
  size_t filter_size_bytes =
      std::max<size_t>(1024, (expected_rule_count * 10 + 7) / 8);

  std::vector<uint8_t> bloom_filter;
  bloom_filter.resize(filter_size_bytes, 0);

  auto populate_bloom =
      [&](const std::map<std::string, std::vector<uint16_t>>& rules_map) {
        for (const auto& [name, indices] : rules_map) {
          if (!name.empty()) {
            std::string_view name_without_prefix =
                std::string_view(name).substr(1);
            SetBloomBits(bloom_filter, GetStyleRuleHash(name_without_prefix));
          }
        }
      };
  populate_bloom(class_map_);
  populate_bloom(id_map_);

  auto bloom_offset = builder_->CreateVector(bloom_filter);

  return flat::CreateStyleRuleIndex(
      *builder_, selectors_offset, domain_map_offset, exclusion_map_offset,
      class_map_offset, id_map_offset, bloom_offset);
}

}  // namespace subresource_filter
