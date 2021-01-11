// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_pattern_index.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/url_pattern_index/ngram_extractor.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_rule_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace url_pattern_index {

namespace {

using FlatUrlRuleList = flatbuffers::Vector<flatbuffers::Offset<flat::UrlRule>>;

using ActivationTypeMap =
    base::flat_map<proto::ActivationType, flat::ActivationType>;
using ElementTypeMap = base::flat_map<proto::ElementType, flat::ElementType>;

// Maps proto::ActivationType to flat::ActivationType.
const ActivationTypeMap& GetActivationTypeMap() {
  static base::NoDestructor<ActivationTypeMap> activation_type_map(
      std::initializer_list<ActivationTypeMap::value_type>{
          {proto::ACTIVATION_TYPE_UNSPECIFIED, flat::ActivationType_NONE},
          {proto::ACTIVATION_TYPE_DOCUMENT, flat::ActivationType_DOCUMENT},
          // ELEMHIDE is not supported.
          {proto::ACTIVATION_TYPE_ELEMHIDE, flat::ActivationType_NONE},
          // GENERICHIDE is not supported.
          {proto::ACTIVATION_TYPE_GENERICHIDE, flat::ActivationType_NONE},
          {proto::ACTIVATION_TYPE_GENERICBLOCK,
           flat::ActivationType_GENERIC_BLOCK},
      });
  return *activation_type_map;
}

// Maps proto::ElementType to flat::ElementType.
const ElementTypeMap& GetElementTypeMap() {
  static base::NoDestructor<ElementTypeMap> element_type_map(
      std::initializer_list<ElementTypeMap::value_type>{
          {proto::ELEMENT_TYPE_UNSPECIFIED, flat::ElementType_NONE},
          {proto::ELEMENT_TYPE_OTHER, flat::ElementType_OTHER},
          {proto::ELEMENT_TYPE_SCRIPT, flat::ElementType_SCRIPT},
          {proto::ELEMENT_TYPE_IMAGE, flat::ElementType_IMAGE},
          {proto::ELEMENT_TYPE_STYLESHEET, flat::ElementType_STYLESHEET},
          {proto::ELEMENT_TYPE_OBJECT, flat::ElementType_OBJECT},
          {proto::ELEMENT_TYPE_XMLHTTPREQUEST,
           flat::ElementType_XMLHTTPREQUEST},
          {proto::ELEMENT_TYPE_OBJECT_SUBREQUEST,
           flat::ElementType_OBJECT_SUBREQUEST},
          {proto::ELEMENT_TYPE_SUBDOCUMENT, flat::ElementType_SUBDOCUMENT},
          {proto::ELEMENT_TYPE_PING, flat::ElementType_PING},
          {proto::ELEMENT_TYPE_MEDIA, flat::ElementType_MEDIA},
          {proto::ELEMENT_TYPE_FONT, flat::ElementType_FONT},
          // Filtering popups is not supported.
          {proto::ELEMENT_TYPE_POPUP, flat::ElementType_NONE},
          {proto::ELEMENT_TYPE_WEBSOCKET, flat::ElementType_WEBSOCKET},
      });
  return *element_type_map;
}

flat::ActivationType ProtoToFlatActivationType(proto::ActivationType type) {
  const auto it = GetActivationTypeMap().find(type);
  DCHECK(it != GetActivationTypeMap().end());
  return it->second;
}

flat::ElementType ProtoToFlatElementType(proto::ElementType type) {
  const auto it = GetElementTypeMap().find(type);
  DCHECK(it != GetElementTypeMap().end());
  return it->second;
}

base::StringPiece ToStringPiece(const flatbuffers::String* string) {
  DCHECK(string);
  return base::StringPiece(string->c_str(), string->size());
}

bool HasNoUpperAscii(base::StringPiece string) {
  return std::none_of(string.begin(), string.end(), base::IsAsciiUpper<char>);
}

// Comparator to sort UrlRule. Sorts rules by descending order of rule priority.
bool UrlRuleDescendingPriorityComparator(const flat::UrlRule* lhs,
                                         const flat::UrlRule* rhs) {
  DCHECK(lhs);
  DCHECK(rhs);
  return lhs->priority() > rhs->priority();
}

// Returns a bitmask of all the keys of the |map| passed.
template <typename T>
int GetKeysMask(const T& map) {
  int mask = 0;
  for (const auto& pair : map)
    mask |= pair.first;
  return mask;
}

// Checks whether a URL |rule| can be converted to its FlatBuffers equivalent,
// and performs the actual conversion.
class UrlRuleFlatBufferConverter {
 public:
  // Creates the converter, and initializes |is_convertible| bit. If
  // |is_convertible| == true, then all the fields, needed for serializing the
  // |rule| to FlatBuffer, are initialized (|options|, |anchor_right|, etc.).
  explicit UrlRuleFlatBufferConverter(const proto::UrlRule& rule)
      : rule_(rule) {
    is_convertible_ = InitializeOptions() && InitializeElementTypes() &&
                      InitializeActivationTypes() && InitializeUrlPattern() &&
                      IsMeaningful();
  }

  // Writes the URL |rule| to the FlatBuffer using the |builder|, and returns
  // the offset to the serialized rule. Returns an empty offset in case the rule
  // can't be converted. The conversion is not possible if the rule has
  // attributes not supported by this client version.
  //
  // |domain_map| Should point to a non-nullptr map of domain vectors to their
  // existing offsets. It is used to de-dupe domain vectors in the serialized
  // rules.
  UrlRuleOffset SerializeConvertedRule(flatbuffers::FlatBufferBuilder* builder,
                                       FlatDomainMap* domain_map) const {
    if (!is_convertible_)
      return UrlRuleOffset();

    DCHECK_NE(rule_.url_pattern_type(), proto::URL_PATTERN_TYPE_REGEXP);

    FlatDomainsOffset domains_included_offset;
    FlatDomainsOffset domains_excluded_offset;
    if (rule_.domains_size()) {
      std::vector<FlatStringOffset> domains_included;
      std::vector<FlatStringOffset> domains_excluded;
      // Reserve only for |domains_included| because it is expected to be the
      // one used more frequently.
      domains_included.reserve(rule_.domains_size());

      for (const auto& domain_list_item : rule_.domains()) {
        const std::string& domain = domain_list_item.domain();

        // Non-ascii characters in domains are unsupported.
        if (!base::IsStringASCII(domain))
          return UrlRuleOffset();

        // Note: This is not always correct. Chrome's URL parser uses upper-case
        // for percent encoded hosts. E.g. https://,.com is encoded as
        // https://%2C.com.
        auto offset = builder->CreateSharedString(
            HasNoUpperAscii(domain) ? domain : base::ToLowerASCII(domain));

        if (domain_list_item.exclude())
          domains_excluded.push_back(offset);
        else
          domains_included.push_back(offset);
      }
      // The domains are stored in sorted order to support fast matching.
      domains_included_offset =
          SerializeDomainList(std::move(domains_included), builder, domain_map);
      domains_excluded_offset =
          SerializeDomainList(std::move(domains_excluded), builder, domain_map);
    }

    // Non-ascii characters in patterns are unsupported.
    if (!base::IsStringASCII(rule_.url_pattern()))
      return UrlRuleOffset();

    // TODO(crbug.com/884063): Lower case case-insensitive patterns here if we
    // want to support case-insensitive rules for subresource filter.
    auto url_pattern_offset = builder->CreateSharedString(rule_.url_pattern());

    return flat::CreateUrlRule(
        *builder, options_, element_types_, activation_types_,
        url_pattern_type_, anchor_left_, anchor_right_, domains_included_offset,
        domains_excluded_offset, url_pattern_offset);
  }

 private:
  FlatDomainsOffset SerializeDomainList(std::vector<FlatStringOffset> domains,
                                        flatbuffers::FlatBufferBuilder* builder,
                                        FlatDomainMap* domain_map) const {
    // The comparator ensuring the domains order necessary for fast matching.
    auto precedes = [&builder](FlatStringOffset lhs, FlatStringOffset rhs) {
      return CompareDomains(
                 ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, lhs)),
                 ToStringPiece(
                     flatbuffers::GetTemporaryPointer(*builder, rhs))) < 0;
    };
    if (domains.empty())
      return FlatDomainsOffset();
    std::sort(domains.begin(), domains.end(), precedes);

    // Share domain lists if we've already serialized an exact duplicate. Note
    // that this can share excluded and included domain lists.
    DCHECK(domain_map);
    auto it = domain_map->find(domains);
    if (it == domain_map->end()) {
      auto offset = builder->CreateVector(domains);
      (*domain_map)[domains] = offset;
      return offset;
    }
    return it->second;
  }

  static bool ConvertAnchorType(proto::AnchorType anchor_type,
                                flat::AnchorType* result) {
    switch (anchor_type) {
      case proto::ANCHOR_TYPE_NONE:
        *result = flat::AnchorType_NONE;
        break;
      case proto::ANCHOR_TYPE_BOUNDARY:
        *result = flat::AnchorType_BOUNDARY;
        break;
      case proto::ANCHOR_TYPE_SUBDOMAIN:
        *result = flat::AnchorType_SUBDOMAIN;
        break;
      default:
        return false;  // Unsupported anchor type.
    }
    return true;
  }

  bool InitializeOptions() {
    static_assert(flat::OptionFlag_ANY <= std::numeric_limits<uint8_t>::max(),
                  "Option flags can not be stored in uint8_t.");

    if (rule_.semantics() == proto::RULE_SEMANTICS_WHITELIST) {
      options_ |= flat::OptionFlag_IS_WHITELIST;
    } else if (rule_.semantics() != proto::RULE_SEMANTICS_BLACKLIST) {
      return false;  // Unsupported semantics.
    }

    switch (rule_.source_type()) {
      case proto::SOURCE_TYPE_ANY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
        FALLTHROUGH;
      case proto::SOURCE_TYPE_FIRST_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_FIRST_PARTY;
        break;
      case proto::SOURCE_TYPE_THIRD_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
        break;

      default:
        return false;  // Unsupported source type.
    }

    // TODO(crbug.com/884063): Consider setting IS_CASE_INSENSITIVE here if we
    // want to support case insensitive rules for subresource_filter.
    return true;
  }

  bool InitializeElementTypes() {
    static_assert(flat::ElementType_ANY <= std::numeric_limits<uint16_t>::max(),
                  "Element types can not be stored in uint16_t.");

    // Handle the default case. Note this means we end up adding
    // flat::ElementType_CSP_REPORT as an element type when there is no
    // corresponding proto::ElementType for it. However this should not matter
    // in practice since subresource_filter does not do matching on CSP reports
    // currently. If subresource_filter started to do so, add support for CSP
    // reports in proto::ElementType.
    if (rule_.element_types() == kDefaultProtoElementTypesMask) {
      element_types_ = kDefaultFlatElementTypesMask;
      return true;
    }

    const ElementTypeMap& element_type_map = GetElementTypeMap();
    // Ensure all proto::ElementType(s) are mapped in |element_type_map|.
    DCHECK_EQ(proto::ELEMENT_TYPE_ALL, GetKeysMask(element_type_map));

    element_types_ = flat::ElementType_NONE;

    for (const auto& pair : element_type_map)
      if (rule_.element_types() & pair.first)
        element_types_ |= pair.second;

    // Normally we can not distinguish between the main plugin resource and any
    // other loads it makes. We treat them both as OBJECT requests. Hence an
    // OBJECT request would also match OBJECT_SUBREQUEST rules, but not the
    // the other way round.
    if (element_types_ & flat::ElementType_OBJECT_SUBREQUEST)
      element_types_ |= flat::ElementType_OBJECT;

    return true;
  }

  bool InitializeActivationTypes() {
    static_assert(
        flat::ActivationType_ANY <= std::numeric_limits<uint8_t>::max(),
        "Activation types can not be stored in uint8_t.");

    const ActivationTypeMap& activation_type_map = GetActivationTypeMap();
    // Ensure all proto::ActivationType(s) are mapped in |activation_type_map|.
    DCHECK_EQ(proto::ACTIVATION_TYPE_ALL, GetKeysMask(activation_type_map));

    activation_types_ = flat::ActivationType_NONE;

    for (const auto& pair : activation_type_map)
      if (rule_.activation_types() & pair.first)
        activation_types_ |= pair.second;

    return true;
  }

  bool InitializeUrlPattern() {
    switch (rule_.url_pattern_type()) {
      case proto::URL_PATTERN_TYPE_SUBSTRING:
        url_pattern_type_ = flat::UrlPatternType_SUBSTRING;
        break;
      case proto::URL_PATTERN_TYPE_WILDCARDED:
        url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
        break;

      // TODO(pkalinnikov): Implement REGEXP rules matching.
      case proto::URL_PATTERN_TYPE_REGEXP:
      default:
        return false;  // Unsupported URL pattern type.
    }

    if (!ConvertAnchorType(rule_.anchor_left(), &anchor_left_) ||
        !ConvertAnchorType(rule_.anchor_right(), &anchor_right_)) {
      return false;
    }
    if (anchor_right_ == flat::AnchorType_SUBDOMAIN)
      return false;  // Unsupported right anchor.

    // We disallow patterns like "||*xyz" because it isn't clear how to match
    // them.
    if (anchor_left_ == flat::AnchorType_SUBDOMAIN &&
        (!rule_.url_pattern().empty() && rule_.url_pattern().front() == '*')) {
      return false;
    }

    return true;
  }

  // Returns whether the rule is not a no-op after all the modifications above.
  bool IsMeaningful() const { return element_types_ || activation_types_; }

  const proto::UrlRule& rule_;

  uint8_t options_ = 0;
  uint16_t element_types_ = 0;
  uint8_t activation_types_ = 0;
  flat::UrlPatternType url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
  flat::AnchorType anchor_left_ = flat::AnchorType_NONE;
  flat::AnchorType anchor_right_ = flat::AnchorType_NONE;

  bool is_convertible_ = true;
};

}  // namespace

// Helpers. --------------------------------------------------------------------

bool OffsetVectorCompare::operator()(
    const std::vector<FlatStringOffset>& a,
    const std::vector<FlatStringOffset>& b) const {
  auto compare = [](const FlatStringOffset a_offset,
                    const FlatStringOffset b_offset) {
    DCHECK(!a_offset.IsNull());
    DCHECK(!b_offset.IsNull());
    return a_offset.o < b_offset.o;
  };
  // |lexicographical_compare| is how vector::operator< is implemented.
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                      compare);
}

UrlRuleOffset SerializeUrlRule(const proto::UrlRule& rule,
                               flatbuffers::FlatBufferBuilder* builder,
                               FlatDomainMap* domain_map) {
  DCHECK(builder);
  UrlRuleFlatBufferConverter converter(rule);
  return converter.SerializeConvertedRule(builder, domain_map);
}

int CompareDomains(base::StringPiece lhs_domain, base::StringPiece rhs_domain) {
  if (lhs_domain.size() != rhs_domain.size())
    return lhs_domain.size() > rhs_domain.size() ? -1 : 1;
  return lhs_domain.compare(rhs_domain);
}

// UrlPatternIndexBuilder ------------------------------------------------------

UrlPatternIndexBuilder::UrlPatternIndexBuilder(
    flatbuffers::FlatBufferBuilder* flat_builder)
    : flat_builder_(flat_builder) {
  DCHECK(flat_builder_);
}

UrlPatternIndexBuilder::~UrlPatternIndexBuilder() = default;

void UrlPatternIndexBuilder::IndexUrlRule(UrlRuleOffset offset) {
  DCHECK(offset.o);

  const auto* rule = flatbuffers::GetTemporaryPointer(*flat_builder_, offset);
  DCHECK(rule);

#if DCHECK_IS_ON()
  // Sanity check that the rule does not have fields with non-ascii characters.
  DCHECK(base::IsStringASCII(ToStringPiece(rule->url_pattern())));
  if (rule->domains_included()) {
    for (auto* domain : *rule->domains_included())
      DCHECK(base::IsStringASCII(ToStringPiece(domain)));
  }
  if (rule->domains_excluded()) {
    for (auto* domain : *rule->domains_excluded())
      DCHECK(base::IsStringASCII(ToStringPiece(domain)));
  }

  // Case-insensitive patterns should be lower-cased.
  if (rule->options() & flat::OptionFlag_IS_CASE_INSENSITIVE)
    DCHECK(HasNoUpperAscii(ToStringPiece(rule->url_pattern())));
#endif

  NGram ngram = GetMostDistinctiveNGram(ToStringPiece(rule->url_pattern()));

  if (ngram) {
    ngram_index_[ngram].push_back(offset);
  } else {
    // TODO(pkalinnikov): Index fallback rules as well.
    fallback_rules_.push_back(offset);
  }
}

UrlPatternIndexOffset UrlPatternIndexBuilder::Finish() {
  std::vector<flatbuffers::Offset<flat::NGramToRules>> flat_hash_table(
      ngram_index_.table_size());

  flatbuffers::Offset<flat::NGramToRules> empty_slot_offset =
      flat::CreateNGramToRules(*flat_builder_);
  auto rules_comparator = [this](const UrlRuleOffset& lhs,
                                 const UrlRuleOffset& rhs) {
    return UrlRuleDescendingPriorityComparator(
        flatbuffers::GetTemporaryPointer(*flat_builder_, lhs),
        flatbuffers::GetTemporaryPointer(*flat_builder_, rhs));
  };

  for (size_t i = 0, size = ngram_index_.table_size(); i != size; ++i) {
    const uint32_t entry_index = ngram_index_.hash_table()[i];
    if (entry_index >= ngram_index_.size()) {
      flat_hash_table[i] = empty_slot_offset;
      continue;
    }
    const MutableNGramIndex::EntryType& entry =
        ngram_index_.entries()[entry_index];
    // Retrieve a mutable reference to |entry.second| and sort it in descending
    // order of priority.
    MutableUrlRuleList& rule_list = ngram_index_[entry.first];
    std::sort(rule_list.begin(), rule_list.end(), rules_comparator);

    auto rules_offset = flat_builder_->CreateVector(rule_list);
    flat_hash_table[i] =
        flat::CreateNGramToRules(*flat_builder_, entry.first, rules_offset);
  }
  auto ngram_index_offset = flat_builder_->CreateVector(flat_hash_table);

  // Sort |fallback_rules_| in descending order of priority.
  std::sort(fallback_rules_.begin(), fallback_rules_.end(), rules_comparator);
  auto fallback_rules_offset = flat_builder_->CreateVector(fallback_rules_);

  return flat::CreateUrlPatternIndex(*flat_builder_, kNGramSize,
                                     ngram_index_offset, empty_slot_offset,
                                     fallback_rules_offset);
}

NGram UrlPatternIndexBuilder::GetMostDistinctiveNGram(
    base::StringPiece pattern) {
  size_t min_list_size = std::numeric_limits<size_t>::max();
  NGram best_ngram = 0;

  // To support case-insensitive matching, make sure the n-grams for |pattern|
  // are lower-cased.
  DCHECK(base::IsStringASCII(pattern));
  auto ngrams =
      CreateNGramExtractor<kNGramSize, NGram, NGramCaseExtraction::kLowerCase>(
          pattern, [](char c) { return c == '*' || c == '^'; });

  for (uint64_t ngram : ngrams) {
    const MutableUrlRuleList* rules = ngram_index_.Get(ngram);
    const size_t list_size = rules ? rules->size() : 0;
    if (list_size < min_list_size) {
      // TODO(pkalinnikov): Pick random of the same-sized lists.
      min_list_size = list_size;
      best_ngram = ngram;
      if (list_size == 0)
        break;
    }
  }

  return best_ngram;
}

// UrlPatternIndex -------------------------------------------------------------

namespace {

using FlatNGramIndex =
    flatbuffers::Vector<flatbuffers::Offset<flat::NGramToRules>>;

// Returns the size of the longest (sub-)domain of |origin| matching one of the
// |domains| in the list.
//
// The |domains| should be sorted in descending order of their length, and
// ascending alphabetical order within the groups of same-length domains.
size_t GetLongestMatchingSubdomain(const url::Origin& origin,
                                   const FlatDomains& domains) {
  // If the |domains| list is short, then the simple strategy is usually faster.
  if (domains.size() <= 5) {
    for (auto* domain : domains) {
      const base::StringPiece domain_piece = ToStringPiece(domain);
      if (origin.DomainIs(domain_piece))
        return domain_piece.size();
    }
    return 0;
  }
  // Otherwise look for each subdomain of the |origin| using binary search.

  DCHECK(!origin.opaque());
  base::StringPiece canonicalized_host(origin.host());
  if (canonicalized_host.empty())
    return 0;

  // If the host name ends with a dot, then ignore it.
  if (canonicalized_host.back() == '.')
    canonicalized_host.remove_suffix(1);

  // The |left| bound of the search is shared between iterations, because
  // subdomains are considered in decreasing order of their lengths, therefore
  // each consecutive lower_bound will be at least as far as the previous.
  flatbuffers::uoffset_t left = 0;
  for (size_t position = 0;; ++position) {
    const base::StringPiece subdomain = canonicalized_host.substr(position);

    flatbuffers::uoffset_t right = domains.size();
    while (left + 1 < right) {
      auto middle = left + (right - left) / 2;
      DCHECK_LT(middle, domains.size());
      if (CompareDomains(ToStringPiece(domains[middle]), subdomain) <= 0)
        left = middle;
      else
        right = middle;
    }

    DCHECK_LT(left, domains.size());
    if (ToStringPiece(domains[left]) == subdomain)
      return subdomain.size();

    position = canonicalized_host.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }

  return 0;
}

// |sorted_candidates| is sorted in descending order by priority. If
// |matched_rules| is specified, then all rule matches in |sorted_candidates|
// will be added to |matched_rules| and null is returned. If |matched_rules| is
// not specified, then this returns the first matching rule i.e. the rule with
// the highest priority in |sorted_candidates| or null if no rule matches.
const flat::UrlRule* FindMatchAmongCandidates(
    const FlatUrlRuleList* sorted_candidates,
    const UrlPattern::UrlInfo& url,
    const url::Origin& document_origin,
    flat::ElementType element_type,
    flat::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules,
    std::vector<const flat::UrlRule*>* matched_rules) {
  if (!sorted_candidates)
    return nullptr;

  DCHECK(std::is_sorted(sorted_candidates->begin(), sorted_candidates->end(),
                        &UrlRuleDescendingPriorityComparator));

  for (const flat::UrlRule* rule : *sorted_candidates) {
    DCHECK_NE(rule, nullptr);
    DCHECK_NE(rule->url_pattern_type(), flat::UrlPatternType_REGEXP);
    if (!DoesRuleFlagsMatch(*rule, element_type, activation_type,
                            is_third_party)) {
      continue;
    }
    if (!UrlPattern(*rule).MatchesUrl(url))
      continue;

    if (DoesOriginMatchDomainList(document_origin, *rule,
                                  disable_generic_rules)) {
      if (matched_rules)
        matched_rules->push_back(rule);
      else
        return rule;
    }
  }

  return nullptr;
}

// Returns whether the network request matches a UrlPattern |index| represented
// in its FlatBuffers format. |is_third_party| should reflect the relation
// between |url| and |document_origin|. If |strategy| is kAll, then
// |matched_rules| will be populated with all matching UrlRules and nullptr is
// returned.
const flat::UrlRule* FindMatchInFlatUrlPatternIndex(
    const flat::UrlPatternIndex& index,
    const UrlPattern::UrlInfo& url,
    const url::Origin& document_origin,
    flat::ElementType element_type,
    flat::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules,
    UrlPatternIndexMatcher::FindRuleStrategy strategy,
    std::vector<const flat::UrlRule*>* matched_rules) {
  using FindRuleStrategy = UrlPatternIndexMatcher::FindRuleStrategy;

  // Check that the outparam |matched_rules| is specified if and only if
  // |strategy| is kAll.
  DCHECK_EQ(strategy == FindRuleStrategy::kAll, !!matched_rules);

  const FlatNGramIndex* hash_table = index.ngram_index();
  const flat::NGramToRules* empty_slot = index.ngram_index_empty_slot();
  DCHECK_NE(hash_table, nullptr);

  NGramHashTableProber prober;

  // |hash_table| contains lower-cased n-grams. Use lower-cased extraction to
  // find prospective matches.
  auto ngrams = CreateNGramExtractor<kNGramSize, uint64_t,
                                     NGramCaseExtraction::kLowerCase>(
      url.spec(), [](char) { return false; });

  auto get_max_priority_rule = [](const flat::UrlRule* lhs,
                                  const flat::UrlRule* rhs) {
    if (!lhs)
      return rhs;
    if (!rhs)
      return lhs;
    return lhs->priority() > rhs->priority() ? lhs : rhs;
  };
  const flat::UrlRule* max_priority_rule = nullptr;

  for (uint64_t ngram : ngrams) {
    const size_t slot_index = prober.FindSlot(
        ngram, base::strict_cast<size_t>(hash_table->size()),
        [hash_table, empty_slot](NGram ngram, size_t slot_index) {
          const flat::NGramToRules* entry = hash_table->Get(slot_index);
          DCHECK_NE(entry, nullptr);
          return entry == empty_slot || entry->ngram() == ngram;
        });
    DCHECK_LT(slot_index, hash_table->size());

    const flat::NGramToRules* entry = hash_table->Get(slot_index);
    if (entry == empty_slot)
      continue;
    const flat::UrlRule* rule = FindMatchAmongCandidates(
        entry->rule_list(), url, document_origin, element_type, activation_type,
        is_third_party, disable_generic_rules, matched_rules);
    if (!rule)
      continue;

    // |rule| is a matching rule with the highest priority amongst
    // |entry->rule_list()|.
    switch (strategy) {
      case FindRuleStrategy::kAny:
        return rule;
      case FindRuleStrategy::kHighestPriority:
        max_priority_rule = get_max_priority_rule(max_priority_rule, rule);
        break;
      case FindRuleStrategy::kAll:
        continue;
    }
  }

  const flat::UrlRule* rule = FindMatchAmongCandidates(
      index.fallback_rules(), url, document_origin, element_type,
      activation_type, is_third_party, disable_generic_rules, matched_rules);

  switch (strategy) {
    case FindRuleStrategy::kAny:
      return rule;
    case FindRuleStrategy::kHighestPriority:
      return get_max_priority_rule(max_priority_rule, rule);
    case FindRuleStrategy::kAll:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

bool DoesOriginMatchDomainList(const url::Origin& origin,
                               const flat::UrlRule& rule,
                               bool disable_generic_rules) {
  const bool is_generic = !rule.domains_included();
  DCHECK(is_generic || rule.domains_included()->size());
  if (disable_generic_rules && is_generic)
    return false;

  // Unique |origin| matches lists of exception domains only.
  if (origin.opaque())
    return is_generic;

  size_t longest_matching_included_domain_length = 1;
  if (!is_generic) {
    longest_matching_included_domain_length =
        GetLongestMatchingSubdomain(origin, *rule.domains_included());
  }
  if (longest_matching_included_domain_length && rule.domains_excluded()) {
    return GetLongestMatchingSubdomain(origin, *rule.domains_excluded()) <
           longest_matching_included_domain_length;
  }
  return !!longest_matching_included_domain_length;
}

bool DoesRuleFlagsMatch(const flat::UrlRule& rule,
                        flat::ElementType element_type,
                        flat::ActivationType activation_type,
                        bool is_third_party) {
  DCHECK((element_type == flat::ElementType_NONE) !=
         (activation_type == flat::ActivationType_NONE));

  if (element_type != flat::ElementType_NONE &&
      !(rule.element_types() & element_type)) {
    return false;
  }
  if (activation_type != flat::ActivationType_NONE &&
      !(rule.activation_types() & activation_type)) {
    return false;
  }

  if (is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_THIRD_PARTY)) {
    return false;
  }
  if (!is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_FIRST_PARTY)) {
    return false;
  }

  return true;
}

UrlPatternIndexMatcher::UrlPatternIndexMatcher(
    const flat::UrlPatternIndex* flat_index)
    : flat_index_(flat_index) {
  DCHECK(!flat_index || flat_index->n() == kNGramSize);
}

UrlPatternIndexMatcher::~UrlPatternIndexMatcher() = default;
UrlPatternIndexMatcher::UrlPatternIndexMatcher(UrlPatternIndexMatcher&&) =
    default;
UrlPatternIndexMatcher& UrlPatternIndexMatcher::operator=(
    UrlPatternIndexMatcher&&) = default;

size_t UrlPatternIndexMatcher::GetRulesCount() const {
  if (rules_count_)
    return *rules_count_;

  if (!flat_index_) {
    rules_count_ = 0;
    return 0;
  }

  rules_count_ = flat_index_->fallback_rules()->size();

  // Iterate over all ngrams and check their corresponding rules.
  for (auto* ngram_to_rules : *flat_index_->ngram_index()) {
    if (ngram_to_rules == flat_index_->ngram_index_empty_slot())
      continue;

    *rules_count_ += ngram_to_rules->rule_list()->size();
  }

  return *rules_count_;
}

const flat::UrlRule* UrlPatternIndexMatcher::FindMatch(
    const GURL& url,
    const url::Origin& first_party_origin,
    proto::ElementType element_type,
    proto::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules,
    FindRuleStrategy strategy) const {
  return FindMatch(url, first_party_origin,
                   ProtoToFlatElementType(element_type),
                   ProtoToFlatActivationType(activation_type), is_third_party,
                   disable_generic_rules, strategy);
}

const flat::UrlRule* UrlPatternIndexMatcher::FindMatch(
    const GURL& url,
    const url::Origin& first_party_origin,
    flat::ElementType element_type,
    flat::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules,
    FindRuleStrategy strategy) const {
  // Ignore URLs that are greater than the max URL length. Since those will be
  // disallowed elsewhere in the loading stack, we can save compute time by
  // avoiding matching here.
  if (!flat_index_ || !url.is_valid() ||
      url.spec().length() > url::kMaxURLChars) {
    return nullptr;
  }
  if ((element_type == flat::ElementType_NONE) ==
      (activation_type == flat::ActivationType_NONE)) {
    return nullptr;
  }

  // FindAllMatches should be used instead to find all matches.
  DCHECK_NE(strategy, FindRuleStrategy::kAll);

  auto* rule = FindMatchInFlatUrlPatternIndex(
      *flat_index_, UrlPattern::UrlInfo(url), first_party_origin, element_type,
      activation_type, is_third_party, disable_generic_rules, strategy,
      nullptr /* matched_rules */);
  if (rule) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                 "UrlPatternIndexMatcher::FindMatch", "pattern",
                 FlatUrlRuleToFilterlistString(rule));
  }
  return rule;
}

std::vector<const flat::UrlRule*> UrlPatternIndexMatcher::FindAllMatches(
    const GURL& url,
    const url::Origin& first_party_origin,
    proto::ElementType element_type,
    proto::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules) const {
  return FindAllMatches(url, first_party_origin,
                        ProtoToFlatElementType(element_type),
                        ProtoToFlatActivationType(activation_type),
                        is_third_party, disable_generic_rules);
}

std::vector<const flat::UrlRule*> UrlPatternIndexMatcher::FindAllMatches(
    const GURL& url,
    const url::Origin& first_party_origin,
    flat::ElementType element_type,
    flat::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules) const {
  // Ignore URLs that are greater than the max URL length. Since those will be
  // disallowed elsewhere in the loading stack, we can save compute time by
  // avoiding matching here.
  if (!flat_index_ || !url.is_valid() ||
      url.spec().length() > url::kMaxURLChars) {
    return std::vector<const flat::UrlRule*>();
  }
  if ((element_type == flat::ElementType_NONE) ==
      (activation_type == flat::ActivationType_NONE)) {
    return std::vector<const flat::UrlRule*>();
  }

  std::vector<const flat::UrlRule*> rules;
  FindMatchInFlatUrlPatternIndex(
      *flat_index_, UrlPattern::UrlInfo(url), first_party_origin, element_type,
      activation_type, is_third_party, disable_generic_rules,
      FindRuleStrategy::kAll, &rules);

  return rules;
}

}  // namespace url_pattern_index
