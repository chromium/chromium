// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

FormFieldTest::FormFieldTest() = default;
FormFieldTest::~FormFieldTest() = default;

void FormFieldTest::AddFormFieldData(std::string control_type,
                                     std::string name,
                                     std::string label,
                                     ServerFieldType expected_type) {
  FormFieldData field_data;
  field_data.form_control_type = control_type;
  field_data.name = base::UTF8ToUTF16(name);
  field_data.label = base::UTF8ToUTF16(label);
  field_data.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field_data));
  expected_classifications_.insert(
      std::make_pair(field_data.unique_renderer_id, expected_type));
}

// Convenience wrapper for text control elements.
void FormFieldTest::AddTextFormFieldData(
    std::string name,
    std::string label,
    ServerFieldType expected_classification) {
  AddFormFieldData("text", name, label, expected_classification);
}

// Apply parsing and verify the expected types.
// |parsed| indicates if at least one field could be parsed successfully.
// |page_language| the language to be used for parsing, default empty value
// means the language is unknown and patterns of all languages are used.
void FormFieldTest::ClassifyAndVerify(ParseResult parse_result,
                                      const LanguageCode& page_language) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner, page_language);

  if (parse_result == ParseResult::NOT_PARSED) {
    ASSERT_EQ(nullptr, field_.get());
    return;
  }
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  for (const std::pair<FieldRendererId, ServerFieldType> it :
       expected_classifications_) {
    ASSERT_TRUE(field_candidates_map_.find(it.first) !=
                field_candidates_map_.end());
    EXPECT_EQ(it.second, field_candidates_map_[it.first].BestHeuristicType());
  }
}

FieldRendererId FormFieldTest::MakeFieldRendererId() {
  return FieldRendererId(++id_counter_);
}
}  // namespace autofill
