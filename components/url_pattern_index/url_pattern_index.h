// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_INDEX_H_
#define COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/url_pattern_index/closed_hash_map.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/uint64_hasher.h"
#include "components/url_pattern_index/url_pattern.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

class GURL;

namespace url {
class Origin;
}

namespace url_pattern_index {

// The integer type used to represent N-grams.
using NGram = uint64_t;
// The hasher used for hashing N-grams.
using NGramHasher = Uint64ToUint32Hasher;
// The hash table probe sequence used both by UrlPatternIndex and its builder.
using NGramHashTableProber = DefaultProber<NGram, NGramHasher>;

// FlatBuffer offset aliases.
using UrlRuleOffset = flatbuffers::Offset<flat::UrlRule>;
using UrlPatternIndexOffset = flatbuffers::Offset<flat::UrlPatternIndex>;

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;
using FlatDomains = flatbuffers::Vector<FlatStringOffset>;
using FlatDomainsOffset = flatbuffers::Offset<FlatDomains>;

struct OffsetVectorCompare {
  bool operator()(const std::vector<FlatStringOffset>& a,
                  const std::vector<FlatStringOffset>& b) const;
};
using FlatDomainMap = std::
    map<std::vector<FlatStringOffset>, FlatDomainsOffset, OffsetVectorCompare>;

constexpr size_t kNGramSize = 5;
static_assert(kNGramSize <= sizeof(NGram), "NGram type is too narrow.");

// The default element types mask as specified by the flatbuffer schema.
constexpr uint16_t kDefaultFlatElementTypesMask =
    flat::ElementType_ANY & ~flat::ElementType_MAIN_FRAME;

// The default element types mask used by a proto::UrlRule.
constexpr uint32_t kDefaultProtoElementTypesMask =
    proto::ELEMENT_TYPE_ALL & ~proto::ELEMENT_TYPE_POPUP;

// Serializes the |rule| to the FlatBuffer |builder|, and returns an offset to
// it in the resulting buffer. Returns null offset iff the |rule| could not be
// serialized because of unsupported options or it is otherwise invalid.
//
// |domain_map| Should point to a non-nullptr map of domain vectors to their
// existing offsets. It is used to de-dupe domain vectors in the serialized
// rules.
UrlRuleOffset SerializeUrlRule(const proto::UrlRule& rule,
                               flatbuffers::FlatBufferBuilder* builder,
                               FlatDomainMap* domain_map);

// Performs three-way comparison between two domains. In the total order defined
// by this predicate, the lengths of domains will be monotonically decreasing.
// Domains of same length are ordered in lexicographic order.
// Returns a negative value if |lhs_domain| should be ordered before
// |rhs_domain|, zero if |lhs_domain| is equal to |rhs_domain| and a positive
// value if |lhs_domain| should be ordered after |rhs_domain|.
int CompareDomains(std::string_view lhs_domain, std::string_view rhs_domain);

// The current format version of UrlPatternIndex.
// Increase this value when introducing an incompatible change to the
// UrlPatternIndex schema (flat/url_pattern_index.fbs). url_pattern_index
// clients can use this as a signal to rebuild rulesets.
constexpr int kUrlPatternIndexFormatVersion = 15;

// The class used to construct an index over the URL patterns of a set of URL
// rules. The rules themselves need to be converted to FlatBuffers format by the
// client of this class, as well as persisted into the |flat_builder| that is
// supplied in the constructor.
class UrlPatternIndexBuilder {
 public:
  explicit UrlPatternIndexBuilder(flatbuffers::FlatBufferBuilder* flat_builder);

  UrlPatternIndexBuilder(const UrlPatternIndexBuilder&) = delete;
  UrlPatternIndexBuilder& operator=(const UrlPatternIndexBuilder&) = delete;

  ~UrlPatternIndexBuilder();

  // Adds a UrlRule to the index. The caller should have already persisted the
  // rule into the same |flat_builder| by a call to SerializeUrlRule returning a
  // non-null |offset|, and should pass in the resulting |offset| here.
  void IndexUrlRule(UrlRuleOffset offset);

  // Finalizes construction of the index, serializes it using |flat_builder|,
  // and returns an offset to it in the resulting FlatBuffer.
  UrlPatternIndexOffset Finish();

 private:
  using MutableUrlRuleList = std::vector<UrlRuleOffset>;
  using MutableNGramIndex =
      ClosedHashMap<NGram, MutableUrlRuleList, NGramHashTableProber>;

  // Returns an N-gram of the |pattern| encoded into the NGram integer type. The
  // N-gram is picked using a greedy heuristic, i.e. the one is chosen which
  // corresponds to the shortest list of rules within the index. If there are no
  // valid N-grams in the |pattern|, the return value is 0.
  NGram GetMostDistinctiveNGram(std::string_view pattern);

  // This index contains all non-REGEXP rules that have at least one acceptable
  // N-gram. For each given rule, the N-gram used as an index key is picked
  // greedily (see GetMostDistinctiveNGram).
  MutableNGramIndex ngram_index_;

  // A fallback list that contains all the rules with no acceptable N-gram.
  MutableUrlRuleList fallback_rules_;

  // Must outlive this instance.
  raw_ptr<flatbuffers::FlatBufferBuilder> flat_builder_;
};

// Encapsulates a read-only index built over the URL patterns of a set of URL
// rules, and provides fast matching of network requests against these rules.
class UrlPatternIndexMatcher {
 public:
  enum class FindRuleStrategy {
    // Any rule is returned in case multiple rules match.
    kAny,

    // If multiple rules match, any of the rules with the highest priority is
    // returned.
    kHighestPriority,

    // All matching rules are returned.
    kAll,
  };

  // Matches the request against `embedder_conditions` and returns true if the
  // request matched.
  using EmbedderConditionsMatcher = base::RepeatingCallback<bool(
      const flatbuffers::Vector<uint8_t>& embedder_conditions)>;

  // Creates an instance to access the given |flat_index|. If |flat_index| is
  // nullptr, then all requests return no match.
  explicit UrlPatternIndexMatcher(const flat::UrlPatternIndex* flat_index);

  UrlPatternIndexMatcher(const UrlPatternIndexMatcher&) = delete;
  UrlPatternIndexMatcher& operator=(const UrlPatternIndexMatcher&) = delete;

  ~UrlPatternIndexMatcher();
  UrlPatternIndexMatcher(UrlPatternIndexMatcher&&);
  UrlPatternIndexMatcher& operator=(UrlPatternIndexMatcher&&);

  // Returns the number of rules in this index. Lazily computed, the first call
  // to this method will scan the entire index.
  size_t GetRulesCount() const;

  // If the index contains one or more UrlRules that match the request, returns
  // one of them, depending on the `strategy`. Otherwise, returns nullptr.
  //
  // Notes on parameters:
  //  - `url` should be valid and not longer than url::kMaxURLChars, otherwise
  //    the return value is nullptr. The length limit is chosen due to
  //    performance implications of matching giant URLs, along with the fact
  //    that in many places in Chrome (e.g. at the IPC layer), URLs longer than
  //    this are dropped already.
  //  - Exactly one of `element_type` and `activation_type` should be specified,
  //    i.e., not equal to *_UNSPECIFIED, otherwise the return value is nullptr.
  //  - `request_method` can only be specified when using flat::* types. Matches
  //    are not filtered by request method when using proto::* types.
  //  - `is_third_party` should be pre-computed by the caller, e.g. using the
  //    registry_controlled_domains library, to reflect the relation between
  //    `url` and `first_party_origin`.
  //
  // A rule is deemed to match the request iff all of the following applies:
  //  - The `url` matches the rule's UrlPattern (see url_pattern.h).
  //  - The `first_party_origin` matches the rule's targeted domains list.
  //  - `element_type` or `activation_type` is among the rule's targeted types.
  //  - The `is_third_party` bit matches the rule's requirement on the requested
  //    `url` being first-/third-party w.r.t. its `first_party_origin`.
  //  - The rule is not generic if `disable_generic_rules` is true.
  const flat::UrlRule* FindMatch(
      const GURL& url,
      const url::Origin& first_party_origin,
      proto::ElementType element_type,
      proto::ActivationType activation_type,
      bool is_third_party,
      bool disable_generic_rules,
      const EmbedderConditionsMatcher& embedder_conditions_matcher,
      FindRuleStrategy strategy,
      const base::flat_set<int>& disabled_rule_ids) const;

  // Helper function to work with flat::*Type(s). If the index contains one or
  // more UrlRules that match the request, returns one of them depending on
  // |strategy|. Otherwise, returns nullptr.
  const flat::UrlRule* FindMatch(
      const GURL& url,
      const url::Origin& first_party_origin,
      flat::ElementType element_type,
      flat::ActivationType activation_type,
      flat::RequestMethod request_method,
      bool is_third_party,
      bool disable_generic_rules,
      const EmbedderConditionsMatcher& embedder_conditions_matcher,
      FindRuleStrategy strategy,
      const base::flat_set<int>& disabled_rule_ids) const;

  // Same as FindMatch, except this function returns all UrlRules that match the
  // request for the index. If no UrlRules match, returns an empty vector.
  std::vector<const flat::UrlRule*> FindAllMatches(
      const GURL& url,
      const url::Origin& first_party_origin,
      proto::ElementType element_type,
      proto::ActivationType activation_type,
      bool is_third_party,
      bool disable_generic_rules,
      const EmbedderConditionsMatcher& embedder_conditions_matcher,
      const base::flat_set<int>& disabled_rule_ids) const;

  // Helper function to work with flat::*Type(s). Returns all UrlRules that
  // match the request for the index. If no UrlRules match, returns an empty
  // vector.
  std::vector<const flat::UrlRule*> FindAllMatches(
      const GURL& url,
      const url::Origin& first_party_origin,
      flat::ElementType element_type,
      flat::ActivationType activation_type,
      flat::RequestMethod request_method,
      bool is_third_party,
      bool disable_generic_rules,
      const EmbedderConditionsMatcher& embedder_conditions_matcher,
      const base::flat_set<int>& disabled_rule_ids) const;

 private:
  // Must outlive this instance.
  raw_ptr<const flat::UrlPatternIndex> flat_index_;

  // The number of rules in this index. Mutable since this is lazily computed.
  mutable std::optional<size_t> rules_count_;
};

// Returns whether the `rule` is considered "generic". A generic rule is one
// whose initator domain list is either empty or contains only negative domains.
bool IsRuleGeneric(const flat::UrlRule& rule);

// Returns whether the `origin` matches the initiator domain list of the `rule`.
// A match means that the longest domain in `domains` that `origin` is a
// sub-domain of is not an exception OR all the `domains` are exceptions and
// neither matches the `origin`. Thus, domain filters with more domain
// components trump filters with fewer domain components, i.e. the more specific
// a filter is, the higher the priority.
bool DoesOriginMatchInitiatorDomainList(const url::Origin& origin,
                                        const flat::UrlRule& rule);

// Returns whether the request URL matches the request domain list of the
// `rule`. See `DoesOriginMatchInitiatorDomainList` for an explanation of the
// matching logic.
bool DoesURLMatchRequestDomainList(const UrlPattern::UrlInfo& url,
                                   const flat::UrlRule& rule);

// Returns whether the request matches flags of the specified `rule`. Takes into
// account:
//  - `element_type` of the requested resource, if not *_NONE.
//  - `activation_type` for a subdocument request, if not *_NONE.
//  - `request_method` of the request, if not *_NONE.
//  - Whether the resource `is_third_party` w.r.t. its embedding document.
//  - Options specified by the embedder via `embedder_conditions_matcher`.
bool DoesRuleFlagsMatch(const flat::UrlRule& rule,
                        flat::ElementType element_type,
                        flat::ActivationType activation_type,
                        flat::RequestMethod request_method,
                        bool is_third_party,
                        const UrlPatternIndexMatcher::EmbedderConditionsMatcher&
                            embedder_conditions_matcher);

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_INDEX_H_
