// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {
namespace proto = url_pattern_index::proto;
using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;
using EmbedderConditionsMatcher =
    url_pattern_index::UrlPatternIndexMatcher::EmbedderConditionsMatcher;

// A helper function to get the checksum on a data buffer.
int LocalGetChecksum(base::span<const uint8_t> data) {
  uint32_t hash = base::PersistentHash(data);

  // Strip off the sign bit since this needs to be persisted in preferences
  // which don't support unsigned ints.
  return static_cast<int>(hash & 0x7fffffff);
}

VerifyStatus GetVerifyStatus(base::span<const uint8_t> buffer,
                             int expected_checksum) {
  // TODO(ericrobinson): Remove the verifier once we've updated the ruleset at
  // least once.  The verifier detects a subset of the errors detected by the
  // checksum, and is unneeded once expected_checksum is consistently nonzero.
  flatbuffers::Verifier verifier(buffer.data(), buffer.size());
  if (expected_checksum != 0 && expected_checksum != LocalGetChecksum(buffer)) {
    return flat::VerifyIndexedRulesetBuffer(verifier)
               ? VerifyStatus::kChecksumFailVerifierPass
               : VerifyStatus::kChecksumFailVerifierFail;
  }
  if (!flat::VerifyIndexedRulesetBuffer(verifier)) {
    return expected_checksum == 0 ? VerifyStatus::kVerifierFailChecksumZero
                                  : VerifyStatus::kVerifierFailChecksumPass;
  }
  return expected_checksum == 0 ? VerifyStatus::kPassChecksumZero
                                : VerifyStatus::kPassValidChecksum;
}

}  // namespace

// RulesetIndexer --------------------------------------------------------------

const int RulesetIndexer::kIndexedFormatVersion = 36;

// This static assert is meant to catch cases where
// url_pattern_index::kUrlPatternIndexFormatVersion is incremented without
// updating RulesetIndexer::kIndexedFormatVersion.
static_assert(url_pattern_index::kUrlPatternIndexFormatVersion == 15,
              "kUrlPatternIndexFormatVersion has changed, make sure you've "
              "also updated RulesetIndexer::kIndexedFormatVersion above.");

RulesetIndexer::RulesetIndexer()
    : blocklist_(&builder_), allowlist_(&builder_), deactivation_(&builder_) {}

RulesetIndexer::~RulesetIndexer() = default;

bool RulesetIndexer::AddUrlRule(const proto::UrlRule& rule) {
  const auto offset =
      url_pattern_index::SerializeUrlRule(rule, &builder_, &domain_map_);
  // Note: A zero offset.o means a "nullptr" offset. It is returned when the
  // rule has not been serialized.
  if (!offset.o)
    return false;

  if (rule.semantics() == proto::RULE_SEMANTICS_BLOCKLIST) {
    blocklist_.IndexUrlRule(offset);
  } else {
    const auto* flat_rule = flatbuffers::GetTemporaryPointer(builder_, offset);
    CHECK(flat_rule, base::NotFatalUntil::M129);
    if (flat_rule->element_types())
      allowlist_.IndexUrlRule(offset);
    if (flat_rule->activation_types())
      deactivation_.IndexUrlRule(offset);
  }

  return true;
}

void RulesetIndexer::Finish() {
  auto blocklist_offset = blocklist_.Finish();
  auto allowlist_offset = allowlist_.Finish();
  auto deactivation_offset = deactivation_.Finish();

  auto url_rules_index_offset = flat::CreateIndexedRuleset(
      builder_, blocklist_offset, allowlist_offset, deactivation_offset);
  builder_.Finish(url_rules_index_offset);
}

int RulesetIndexer::GetChecksum() const {
  return LocalGetChecksum(data());
}

// IndexedRulesetMatcher -------------------------------------------------------

// static
bool IndexedRulesetMatcher::Verify(base::span<const uint8_t> buffer,
                                   int expected_checksum,
                                   std::string_view uma_tag) {
  TRACE_EVENT_BEGIN1(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "IndexedRulesetMatcher::Verify", "size", buffer.size());
  base::ScopedUmaHistogramTimer scoped_timer(
      base::StrCat({uma_tag, ".IndexRuleset.Verify2.WallDuration"}));
  VerifyStatus status = GetVerifyStatus(buffer, expected_checksum);
  base::UmaHistogramEnumeration(
      base::StrCat({uma_tag, ".IndexRuleset.Verify.Status"}), status);
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "IndexedRulesetMatcher::Verify", "status",
                   static_cast<int>(status));
  return status == VerifyStatus::kPassValidChecksum ||
         status == VerifyStatus::kPassChecksumZero;
}

IndexedRulesetMatcher::IndexedRulesetMatcher(base::span<const uint8_t> buffer)
    : root_(flat::GetIndexedRuleset(buffer.data())),
      blocklist_(root_->blocklist_index()),
      allowlist_(root_->allowlist_index()),
      deactivation_(root_->deactivation_index()) {}

bool IndexedRulesetMatcher::ShouldDisableFilteringForDocument(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    proto::ActivationType activation_type) const {
  return !!deactivation_.FindMatch(
      document_url, parent_document_origin, proto::ELEMENT_TYPE_UNSPECIFIED,
      activation_type,
      FirstPartyOrigin::IsThirdParty(document_url, parent_document_origin),
      false, EmbedderConditionsMatcher(), FindRuleStrategy::kAny,
      {} /* disabled_rule_ids */);
}

LoadPolicy IndexedRulesetMatcher::GetLoadPolicyForResourceLoad(
    const GURL& url,
    const FirstPartyOrigin& first_party,
    proto::ElementType element_type,
    bool disable_generic_rules) const {
  const url_pattern_index::flat::UrlRule* rule =
      MatchedUrlRule(url, first_party, element_type, disable_generic_rules);

  if (!rule)
    return LoadPolicy::ALLOW;

  return rule->options() & url_pattern_index::flat::OptionFlag_IS_ALLOWLIST
             ? LoadPolicy::EXPLICITLY_ALLOW
             : LoadPolicy::DISALLOW;
}

const url_pattern_index::flat::UrlRule* IndexedRulesetMatcher::MatchedUrlRule(
    const GURL& url,
    const FirstPartyOrigin& first_party,
    url_pattern_index::proto::ElementType element_type,
    bool disable_generic_rules) const {
  const bool is_third_party = first_party.IsThirdParty(url);
  const EmbedderConditionsMatcher embedder_conditions_matcher;

  auto find_match =
      [&](const url_pattern_index::UrlPatternIndexMatcher& matcher) {
        return matcher.FindMatch(
            url, first_party.origin(), element_type,
            proto::ACTIVATION_TYPE_UNSPECIFIED, is_third_party,
            disable_generic_rules, embedder_conditions_matcher,
            FindRuleStrategy::kAny, {} /* disabled_rule_ids */);
      };

  // Always check the allowlist for subdocuments. For other forms of resources,
  // it is not necessary to differentiate between the resource not matching a
  // blocklist rule and matching an allowlist rule. For subdocuments, matching
  // an allowlist rule can still override ad tagging decisions even if the
  // subdocument url did not match a blocklist rule.
  //
  // To optimize the subdocument case, we only check the blocklist if an
  // allowlist rule was not matched.
  if (element_type == proto::ELEMENT_TYPE_SUBDOCUMENT) {
    auto* allowlist_rule = find_match(allowlist_);
    if (allowlist_rule)
      return allowlist_rule;
    return find_match(blocklist_);
  }

  // For non-subdocument elements, only check the allowlist if there is a
  // matched blocklist rule to prevent unnecessary lookups.
  auto* blocklist_rule = find_match(blocklist_);
  if (!blocklist_rule)
    return nullptr;
  auto* allowlist_rule = find_match(allowlist_);
  return allowlist_rule ? allowlist_rule : blocklist_rule;
}

}  // namespace subresource_filter
