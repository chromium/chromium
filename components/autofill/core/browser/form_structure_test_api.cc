// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_test_api.h"

#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Pair;

// static
void FormStructureTestApi::SetFieldTypes(
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  ASSERT_EQ(form_structure_->field_count(), heuristic_types.size());
  ASSERT_EQ(form_structure_->field_count(), server_types.size());
  ASSERT_THAT(heuristic_types,
              Each(Contains(Pair(GetActivePatternSource(), _))))
      << "There must be a default heuristic prediction for every field.";

  for (size_t i = 0; i < form_structure_->field_count(); ++i) {
    AutofillField* form_field = form_structure_->field(i);
    ASSERT_TRUE(form_field);

    for (const auto& [source, type] : heuristic_types[i])
      form_field->set_heuristic_type(source, type);
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
        prediction;
    prediction.set_type(server_types[i]);
    form_field->set_server_predictions({prediction});
  }

  form_structure_->UpdateAutofillCount();
}

void FormStructureTestApi::SetFieldTypes(
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>
      all_heuristic_types;
  for (ServerFieldType type : heuristic_types)
    all_heuristic_types.push_back({{GetActivePatternSource(), type}});
  SetFieldTypes(all_heuristic_types, server_types);
}

}  // namespace autofill
