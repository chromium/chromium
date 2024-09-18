// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "components/subresource_filter/core/common/flat/indexed_ruleset_generated.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

class GURL;

namespace url {
class Origin;
}

namespace url_pattern_index {
namespace proto {
class UrlRule;
}
}

namespace subresource_filter {

class FirstPartyOrigin;

// Detailed result of IndexedRulesetMatcher::Verify.
// Note: Logged to UMA, keep in sync with SubresourceFilterVerifyStatus in
// enums.xml.  Add new entries to the end and do not renumber.
enum class VerifyStatus {
  kPassValidChecksum = 0,
  kChecksumFailVerifierPass = 1,
  kChecksumFailVerifierFail = 2,
  kVerifierFailChecksumPass = 3,
  kVerifierFailChecksumZero = 4,
  kPassChecksumZero = 5,
  kMaxValue = kPassChecksumZero
};

// The class used to construct flat data structures representing the set of URL
// filtering rules, as well as the index of those. Internally owns a
// FlatBufferBuilder storing the structures.
class RulesetIndexer {
 public:
  // The current binary format version of the indexed ruleset.
  //
  // Increase this value when introducing an incompatible change in
  // IndexedRuleset format, or otherwise willing to nudge clients to rebuild
  // their ruleset (e.g., a change is compatible, but significantly reduces the
  // size of the buffer). Note: The PRESUBMIT.py script tries to keep
  // contributors aware of that.
  static const int kIndexedFormatVersion;

  RulesetIndexer();

  RulesetIndexer(const RulesetIndexer&) = delete;
  RulesetIndexer& operator=(const RulesetIndexer&) = delete;

  ~RulesetIndexer();

  // Adds |rule| to the ruleset and the index unless the |rule| has unsupported
  // filter options, in which case the data structures remain unmodified.
  // Returns whether the |rule| has been serialized and added to the index.
  bool AddUrlRule(const url_pattern_index::proto::UrlRule& rule);

  // Finalizes construction of the data structures.
  void Finish();

  // Returns the checksum for the data buffer.
  int GetChecksum() const;

  // Returns a pointer to the buffer containing the serialized flat data
  // structures. Should only be called after Finish().
  base::span<const uint8_t> data() const LIFETIME_BOUND {
    return base::span(builder_.GetBufferPointer(), builder_.GetSize());
  }

 private:
  flatbuffers::FlatBufferBuilder builder_;

  url_pattern_index::UrlPatternIndexBuilder blocklist_;
  url_pattern_index::UrlPatternIndexBuilder allowlist_;
  url_pattern_index::UrlPatternIndexBuilder deactivation_;

  // Maintains a map of domain vectors to their existing offsets, to avoid
  // storing a particular vector more than once.
  url_pattern_index::FlatDomainMap domain_map_;
};

// Matches URLs against the FlatBuffer representation of an indexed ruleset.
class IndexedRulesetMatcher {
 public:
  // Returns whether the |buffer| of the given |size| contains a valid
  // flat::IndexedRuleset FlatBuffer.
  static bool Verify(base::span<const uint8_t> buffer,
                     int expected_checksum,
                     std::string_view uma_tag);

  // Creates an instance that matches URLs against the flat::IndexedRuleset
  // provided as the root object of serialized data in the |buffer|.
  explicit IndexedRulesetMatcher(base::span<const uint8_t> buffer);

  IndexedRulesetMatcher(const IndexedRulesetMatcher&) = delete;
  IndexedRulesetMatcher& operator=(const IndexedRulesetMatcher&) = delete;

  // Returns whether the subset of subresource filtering rules specified by the
  // |activation_type| should be disabled for the |document| loaded from
  // |parent_document_origin|. Always returns false if |activation_type| ==
  // ACTIVATION_TYPE_UNSPECIFIED or the |document_url| is not valid. Unlike
  // page-level activation, such rules can be used to have fine-grained control
  // over the activation of filtering within (sub-)documents.
  bool ShouldDisableFilteringForDocument(
      const GURL& document_url,
      const url::Origin& parent_document_origin,
      url_pattern_index::proto::ActivationType activation_type) const;

  // Returns the LoadPolicy for a network request to |url| of |element_type|
  // initiated by |document_origin|. Always returns ALLOW if the  |url| is not
  // valid or |element_type| == ELEMENT_TYPE_UNSPECIFIED.
  LoadPolicy GetLoadPolicyForResourceLoad(
      const GURL& url,
      const FirstPartyOrigin& first_party,
      url_pattern_index::proto::ElementType element_type,
      bool disable_generic_rules) const;

  // Like ShouldDisallowResourceLoad, but returns the matching rule that
  // determines whether the request should be allowed or not. Allowlist rules
  // override blocklist rules. If no rule matches, returns nullptr.
  const url_pattern_index::flat::UrlRule* MatchedUrlRule(
      const GURL& url,
      const FirstPartyOrigin& first_party,
      url_pattern_index::proto::ElementType element_type,
      bool disable_generic_rules) const;

 private:
  raw_ptr<const flat::IndexedRuleset> root_;

  url_pattern_index::UrlPatternIndexMatcher blocklist_;
  url_pattern_index::UrlPatternIndexMatcher allowlist_;
  url_pattern_index::UrlPatternIndexMatcher deactivation_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
