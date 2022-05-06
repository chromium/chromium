// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_form_structure.h"

#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Pair;

TestFormStructure::TestFormStructure(const FormData& form)
    : FormStructure(form) {}

TestFormStructure::~TestFormStructure() {}

void TestFormStructure::SetFieldTypes(
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>
      all_heuristic_types;

  base::ranges::transform(
      heuristic_types, std::back_inserter(all_heuristic_types),
      [](ServerFieldType type)
          -> std::vector<std::pair<PatternSource, ServerFieldType>> {
        return {{GetActivePatternSource(), type}};
      });

  SetFieldTypes(all_heuristic_types, server_types);
}

void TestFormStructure::SetFieldTypes(
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  ASSERT_EQ(field_count(), heuristic_types.size());
  ASSERT_EQ(field_count(), server_types.size());
  ASSERT_THAT(heuristic_types,
              Each(Contains(Pair(GetActivePatternSource(), _))))
      << "There must be a default heuristic prediction for every field.";

  for (size_t i = 0; i < field_count(); ++i) {
    AutofillField* form_field = field(i);
    ASSERT_TRUE(form_field);

    for (const auto& [source, type] : heuristic_types[i])
      form_field->set_heuristic_type(source, type);
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
        prediction;
    prediction.set_type(server_types[i]);
    form_field->set_server_predictions({prediction});
  }

  UpdateAutofillCount();
}

}  // namespace autofill
