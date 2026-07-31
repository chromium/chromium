// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

namespace {
// Order reflects their priority in ascending order.
// kSearch has the lowest precedence and kEmail has the highest precedence.
inline constexpr std::array<HeuristicParser, 12>
    kParserIncreasingPriorityOrder = {
        HeuristicParser::kSearch,      HeuristicParser::kMerchantPromoCode,
        HeuristicParser::kName,        HeuristicParser::kLoyaltyCard,
        HeuristicParser::kPrice,       HeuristicParser::kIban,
        HeuristicParser::kOneTimeCode, HeuristicParser::kCreditCard,
        HeuristicParser::kAddress,     HeuristicParser::kTravel,
        HeuristicParser::kPhone,       HeuristicParser::kEmail};

// Returns `parser_type`'s priority as the index in
// `kParserIncreasingPriorityOrder`.
inline constexpr size_t GetParserPriority(HeuristicParser parser_type) {
  for (size_t i = 0; i < kParserIncreasingPriorityOrder.size(); ++i) {
    if (kParserIncreasingPriorityOrder[i] == parser_type) {
      return i;
    }
  }
  NOTREACHED();
}

}  // namespace

FieldCandidatePriority::FieldCandidatePriority(
    bool is_name_or_high_quality_label_match,
    HeuristicParser parser_type)
    : is_name_or_high_quality_label_match(is_name_or_high_quality_label_match),
      parser_priority(GetParserPriority(parser_type)) {}

FieldCandidates::FieldCandidates() = default;

FieldCandidates::FieldCandidates(FieldCandidates&& other) = default;
FieldCandidates& FieldCandidates::operator=(FieldCandidates&& other) = default;

FieldCandidates::~FieldCandidates() = default;

void FieldCandidates::AddFieldCandidate(FieldType type,
                                        MatchInfo match_info,
                                        FieldCandidatePriority priority) {
  field_candidates_.push_back(FieldCandidate{
      .type = type, .match_info = match_info, .priority = priority});
}

// We currently select a type with the maximum score sum.
std::optional<FieldCandidate> FieldCandidates::BestHeuristicCandidate() const {
  if (field_candidates_.empty()) {
    return std::nullopt;
  }

  return *std::ranges::max_element(field_candidates_, {},
                                   &FieldCandidate::priority);
}

DenseSet<MatchAttribute> FieldCandidates::BestHeuristicTypeReason() const {
  DenseSet<MatchAttribute> attributes;
  std::optional<FieldCandidate> best_candidate = BestHeuristicCandidate();
  // Terminate early if there were no candidates.
  if (!best_candidate) {
    return attributes;
  }

  FieldType best_type = best_candidate->type;
  for (const FieldCandidate& candidate : field_candidates_) {
    if (candidate.type == best_type) {
      attributes.insert(candidate.match_info.matched_attribute ==
                                MatchInfo::MatchAttribute::kName
                            ? MatchAttribute::kName
                            : MatchAttribute::kLabel);
    }
  }
  return attributes;
}

}  // namespace autofill
