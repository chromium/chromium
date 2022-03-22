// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

FormFieldTestBase::FormFieldTestBase() = default;
FormFieldTestBase::~FormFieldTestBase() = default;

void FormFieldTestBase::AddFormFieldData(std::string control_type,
                                         std::string name,
                                         std::string label,
                                         ServerFieldType expected_type) {
  AddFormFieldDataWithLength(control_type, name, label, /*max_length=*/0,
                             expected_type);
}

void FormFieldTestBase::AddFormFieldDataWithLength(
    std::string control_type,
    std::string name,
    std::string label,
    int max_length,
    ServerFieldType expected_type) {
  FormFieldData field_data;
  field_data.form_control_type = control_type;
  field_data.name = base::UTF8ToUTF16(name);
  field_data.label = base::UTF8ToUTF16(label);
  field_data.max_length = max_length;
  field_data.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field_data));
  expected_classifications_.insert(
      std::make_pair(field_data.global_id(), expected_type));
}

void FormFieldTestBase::AddSelectOneFormFieldData(
    std::string name,
    std::string label,
    const std::vector<SelectOption>& options,
    ServerFieldType expected_type) {
  AddSelectOneFormFieldDataWithLength(name, label, 0, options, expected_type);
}

void FormFieldTestBase::AddSelectOneFormFieldDataWithLength(
    std::string name,
    std::string label,
    int max_length,
    const std::vector<SelectOption>& options,
    ServerFieldType expected_type) {
  AddFormFieldData("select-one", name, label, expected_type);
  FormFieldData* field_data = list_.back().get();
  field_data->max_length = max_length;
  field_data->options = options;
}

// Convenience wrapper for text control elements.
void FormFieldTestBase::AddTextFormFieldData(
    std::string name,
    std::string label,
    ServerFieldType expected_classification) {
  AddFormFieldData("text", name, label, expected_classification);
}

// Apply parsing and verify the expected types.
// |parsed| indicates if at least one field could be parsed successfully.
// |page_language| the language to be used for parsing, default empty value
// means the language is unknown and patterns of all languages are used.
void FormFieldTestBase::ClassifyAndVerify(ParseResult parse_result,
                                          const LanguageCode& page_language) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner, page_language);

  if (parse_result == ParseResult::NOT_PARSED) {
    ASSERT_EQ(nullptr, field_.get());
    return;
  }
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  TestClassificationExpectations();
}

void FormFieldTestBase::TestClassificationExpectations() {
  for (const auto [field_id, field_type] : expected_classifications_) {
    if (field_type != UNKNOWN_TYPE) {
      SCOPED_TRACE(testing::Message()
                   << "Found type "
                   << AutofillType::ServerFieldTypeToString(
                          field_candidates_map_[field_id].BestHeuristicType())
                   << ", expected type "
                   << AutofillType::ServerFieldTypeToString(field_type));

      ASSERT_TRUE(field_candidates_map_.find(field_id) !=
                  field_candidates_map_.end());
      EXPECT_EQ(field_type,
                field_candidates_map_[field_id].BestHeuristicType());
    } else {
      SCOPED_TRACE(
          testing::Message()
          << "Expected type UNKNOWN_TYPE but got "
          << AutofillType::ServerFieldTypeToString(
                 field_candidates_map_.find(field_id) !=
                         field_candidates_map_.end()
                     ? field_candidates_map_[field_id].BestHeuristicType()
                     : UNKNOWN_TYPE));
      EXPECT_EQ(field_candidates_map_.find(field_id),
                field_candidates_map_.end());
    }
  }
}

FieldRendererId FormFieldTestBase::MakeFieldRendererId() {
  return FieldRendererId(++id_counter_);
}

FormFieldTest::FormFieldTest() = default;
FormFieldTest::~FormFieldTest() = default;
}  // namespace autofill
