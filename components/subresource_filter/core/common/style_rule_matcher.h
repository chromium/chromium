// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_MATCHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_MATCHER_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/sequence_checker.h"
#include "components/subresource_filter/core/common/flat/style_rule_index_generated.h"
#include "components/subresource_filter/core/common/style_rule_bloom_filter.h"
#include "url/origin.h"

namespace subresource_filter {

// StyleRuleMatcher is used for finding relevant filterlist selectors for a
// document. Given a document, call `GetDomainSelectors` to find all
// domain-specific selectors for that document. Then, for each element's class
// and id call `GetSelectorsByClass` and `GetSelectorsById` to see if there
// are any relevant global selectors to add.
class StyleRuleMatcher {
 public:
  // `index` must outlive this instance.
  explicit StyleRuleMatcher(const flat::StyleRuleIndex* index);

  StyleRuleMatcher(const StyleRuleMatcher&) = delete;
  StyleRuleMatcher& operator=(const StyleRuleMatcher&) = delete;

  ~StyleRuleMatcher();

  // Returns whether the ruleset might have any selectors matching the given
  // `hash`.
  bool MaybeHasStyleRule(uint32_t hash) const;

  // Returns the selectors that apply to the `document_origin`.
  void GetDomainSelectors(const url::Origin& document_origin,
                          std::vector<std::string_view>& out_selectors) const;

  // Returns the selectors that contain the `class_name` and apply to the
  // `document_origin`.
  void GetSelectorsByClass(const url::Origin& document_origin,
                           std::string_view class_name,
                           uint32_t hash,
                           std::vector<std::string_view>& out_selectors) const;

  // Returns the selectors that contain the `id_name` and apply to the
  // `document_origin`.
  void GetSelectorsById(const url::Origin& document_origin,
                        std::string_view id_name,
                        uint32_t hash,
                        std::vector<std::string_view>& out_selectors) const;

 private:
  const base::flat_set<uint16_t>& GetExcludedIndices(
      const url::Origin& document_origin) const;

  bool CheckBloomBits(uint32_t hash) const;

  // Helper for GetSelectorsByClass and GetSelectorsById.
  void GetSelectorsForMap(
      const url::Origin& document_origin,
      const flatbuffers::Vector<flatbuffers::Offset<flat::NameToSelectors>>*
          map,
      std::string_view name,
      uint32_t hash,
      std::vector<std::string_view>& out_selectors) const;

  raw_ptr<const flat::StyleRuleIndex> index_;

  // The filter is backed by the `index_` flatbuffer.
  StyleRuleBloomFilter bloom_filter_;

  // The following fields are used to cache the excluded indices for the last
  // queried origin. They are mutable to allow caching in const methods.
  mutable std::optional<base::flat_set<uint16_t>> cached_excluded_indices_;
  mutable std::optional<url::Origin> cached_origin_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_MATCHER_H_
