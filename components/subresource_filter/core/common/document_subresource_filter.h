// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/load_policy.h"
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
  //  -- Filter subresource loads in the scope of a document loaded from
  //     |document_origin|.
  //  -- Hold a reference to and use |ruleset| for its entire lifetime.
  DocumentSubresourceFilter(url::Origin document_origin,
                            mojom::ActivationState activation_state,
                            scoped_refptr<const MemoryMappedRuleset> ruleset);

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
      url_pattern_index::proto::ElementType subresource_type);

  // Returns the matching rule that determines whether the request url and type
  // should be allowed. If no rule matches, returns nullptr.
  const url_pattern_index::flat::UrlRule* FindMatchingUrlRule(
      const GURL& subresource_url,
      url_pattern_index::proto::ElementType subresource_type);

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
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
