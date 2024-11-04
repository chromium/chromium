// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <algorithm>
#include <array>

namespace autofill {

FieldCandidate::FieldCandidate(FieldType field_type, float field_score)
    : type(field_type), score(field_score) {}

FieldCandidates::FieldCandidates() = default;

FieldCandidates::FieldCandidates(FieldCandidates&& other) = default;
FieldCandidates& FieldCandidates::operator=(FieldCandidates&& other) = default;

FieldCandidates::~FieldCandidates() = default;

void FieldCandidates::AddFieldCandidate(FieldType type, float score) {
  field_candidates_.emplace_back(type, score);
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

}  // namespace autofill
