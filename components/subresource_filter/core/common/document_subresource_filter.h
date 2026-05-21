// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/scoped_rule.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace subresource_filter {

class FirstPartyOrigin;
class MemoryMappedRuleset;

// Performs filtering of subresource loads in the scope of a given document.
class DocumentSubresourceFilter {
 public:
  // Constructs a new filter that will:
  //  -- Operate in a manner prescribed in |activation_state|.
  //  -- Filter subresource loads and dom elements in the scope of a document
  //  loaded from
  //     |document_origin|.
  //  -- Hold a reference to and use |ruleset| for its entire lifetime.
  DocumentSubresourceFilter(url::Origin document_origin,
                            mojom::ActivationState activation_state,
                            scoped_refptr<const MemoryMappedRuleset> ruleset,
                            std::string_view uma_tag);

  DocumentSubresourceFilter(const DocumentSubresourceFilter&) = delete;
  DocumentSubresourceFilter& operator=(const DocumentSubresourceFilter&) =
      delete;

  ~DocumentSubresourceFilter();

  const mojom::ActivationState& activation_state() const {
    return activation_state_;
  }
  const mojom::DocumentLoadStatistics& statistics() const {
    return statistics_;
  }

  // WARNING: This is only to allow DocumentSubresourceFilter's wrappers to
  // modify the |statistics|.
  // TODO(pkalinnikov): Find a better way to achieve this.
  mojom::DocumentLoadStatistics& statistics() { return statistics_; }

  LoadPolicy GetLoadPolicy(
      const GURL& subresource_url,
      url_pattern_index::proto::ElementType subresource_type,
      ScopedRule* out_rule = nullptr);

  // Returns the matching rule that determines whether the request url and type
  // should be allowed. If no rule matches, returns nullptr.
  const url_pattern_index::flat::UrlRule* FindMatchingUrlRule(
      const GURL& subresource_url,
      url_pattern_index::proto::ElementType subresource_type);

  // Returns the unique ID of the ruleset currently being used for filtering.
  // Used to partition renderer-side caches by ruleset version.
  uint64_t GetRulesetId() const;

  // Returns true if the ruleset might contain style rules matching the given
  // `hash` (which is a hash of a class or ID name). This is a fast Bloom
  // filter check used to avoid expensive operations for classes/IDs that
  // definitely don't have associated selectors.
  bool MaybeHasStyleRule(uint32_t hash) const;

  // Appends to `out_selectors` the site-specific style selectors that apply to
  // the current document's origin (e.g. "example.com##.ad").
  void GetDomainSelectors(std::vector<std::string_view>& out_selectors) const;

  // Appends to `out_selectors` the global style selectors containing
  // `class_name` that apply to the current document's origin. `hash` must be
  // the hash of `class_name` (computed via GetStyleRuleHash).
  void GetSelectorsByClass(std::string_view class_name,
                           uint32_t hash,
                           std::vector<std::string_view>& out_selectors) const;

  // Appends to `out_selectors` the global style selectors containing
  // `id_name` that apply to the current document's origin. `hash` must be
  // the hash of `id_name` (computed via GetStyleRuleHash).
  void GetSelectorsById(std::string_view id_name,
                        uint32_t hash,
                        std::vector<std::string_view>& out_selectors) const;

  // Called if the DocumentSubresourceFilter needs to change how it filters
  // subresources.
  void set_activation_state(const mojom::ActivationState& state) {
    activation_state_ = state;
  }

 private:
  mojom::ActivationState activation_state_;
  const scoped_refptr<const MemoryMappedRuleset> ruleset_;
  const IndexedRulesetMatcher ruleset_matcher_;

  // Equals nullptr iff |activation_state_.filtering_disabled_for_document|.
  std::unique_ptr<FirstPartyOrigin> document_origin_;

  mojom::DocumentLoadStatistics statistics_;

  std::string_view uma_tag_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
