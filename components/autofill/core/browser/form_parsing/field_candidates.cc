// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <algorithm>
#include <array>

#include "base/logging.h"
#include "components/autofill/core/common/autofill_features.h"

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
  if (field_candidates_.empty())
    return UNKNOWN_TYPE;

  // Scores for each type. The index is their FieldType enum value.
  std::array<float, MAX_VALID_FIELD_TYPE> type_scores;
  type_scores.fill(0.0f);

  for (const auto& field_candidate : field_candidates_) {
    VLOG(1) << "type: " << field_candidate.type
            << " score: " << field_candidate.score;
    type_scores[field_candidate.type] += field_candidate.score;
  }

  const auto best_type_iter = base::ranges::max_element(type_scores);
  const size_t index = std::distance(type_scores.begin(), best_type_iter);

  return ToSafeFieldType(index, NO_SERVER_DATA);
}

}  // namespace autofill
