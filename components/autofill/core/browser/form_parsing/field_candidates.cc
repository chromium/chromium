// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <algorithm>
#include <array>

#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

FieldCandidates::FieldCandidates() = default;

FieldCandidates::FieldCandidates(FieldCandidates&& other) = default;
FieldCandidates& FieldCandidates::operator=(FieldCandidates&& other) = default;

FieldCandidates::~FieldCandidates() = default;

void FieldCandidates::AddFieldCandidate(FieldType type,
                                        MatchAttribute match_attribute,
                                        float score) {
  field_candidates_.push_back(FieldCandidate{
      .type = type, .match_attribute = match_attribute, .score = score});
}

// We currently select a type with the maximum score sum.
FieldType FieldCandidates::BestHeuristicType() const {
  if (field_candidates_.empty()) {
    return UNKNOWN_TYPE;
  }

  std::array<float, MAX_VALID_FIELD_TYPE> type_scores{};
  for (const FieldCandidate& candidate : field_candidates_) {
    type_scores[candidate.type] += candidate.score;
  }

  const auto best_type_it = std::ranges::max_element(type_scores);
  const size_t index = std::distance(type_scores.begin(), best_type_it);
  return ToSafeFieldType(index, NO_SERVER_DATA);
}

DenseSet<MatchAttribute> FieldCandidates::BestHeuristicTypeReason() const {
  FieldType best_type = BestHeuristicType();
  DenseSet<MatchAttribute> attributes;
  for (const FieldCandidate& candidate : field_candidates_) {
    if (candidate.type == best_type) {
      attributes.insert(candidate.match_attribute);
    }
  }
  return attributes;
}

}  // namespace autofill
