// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {
void UpdateRanks(std::vector<std::unique_ptr<AutofillField>>& fields) {
  for (size_t i = 0; i < fields.size(); ++i) {
    fields[i]->set_rank(i);
  }
}
}  // namespace

FormFieldParserTestBase::FormFieldParserTestBase() = default;
FormFieldParserTestBase::~FormFieldParserTestBase() = default;

void FormFieldParserTestBase::AddFormFieldData(FormControlType control_type,
                                               std::string_view name,
                                               std::string_view label,
                                               std::string_view placeholder,
                                               int max_length,
                                               FieldType expected_type) {
  FormFieldData field_data;
  field_data.set_form_control_type(control_type);
  field_data.set_name(base::UTF8ToUTF16(name));
  field_data.set_label(base::UTF8ToUTF16(label));
  field_data.set_placeholder(base::UTF8ToUTF16(placeholder));
  field_data.set_max_length(max_length);
  field_data.set_renderer_id(MakeFieldRendererId());
  fields_.push_back(std::make_unique<AutofillField>(field_data));
  expected_classifications_.insert(
      std::make_pair(field_data.global_id(), expected_type));
}

void FormFieldParserTestBase::AddFormFieldData(FormControlType control_type,
                                               std::string_view name,
                                               std::string_view label,
                                               FieldType expected_type) {
  AddFormFieldData(control_type, name, label, /*placeholder=*/"",
                   /*max_length=*/0, expected_type);
}

void FormFieldParserTestBase::AddSelectOneFormFieldData(
    std::string_view name,
    std::string_view label,
    const std::vector<SelectOption>& options,
    FieldType expected_type) {
  AddFormFieldData(FormControlType::kSelectOne, name, label, expected_type);
  FormFieldData* field_data = fields_.back().get();
  field_data->set_options(options);
}

void FormFieldParserTestBase::AddTextFormFieldData(std::string_view name,
                                                   std::string_view label,
                                                   FieldType expected_type) {
  AddFormFieldData(FormControlType::kInputText, name, label, expected_type);
}

// Apply parsing and verify the expected types.
// |parsed| indicates if at least one field could be parsed successfully.
// |page_language| the language to be used for parsing, default empty value
// means the language is unknown and patterns of all languages are used.
void FormFieldParserTestBase::ClassifyAndVerify(
    ParseResult parse_result,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternFile pattern_file) {
  UpdateRanks(fields_);
  AutofillScanner scanner(fields_);
  ParsingContext context(client_country, page_language, pattern_file);
  std::unique_ptr<FormFieldParser> field = Parse(context, &scanner);

  if (parse_result == ParseResult::kNotParsed) {
    ASSERT_EQ(nullptr, field.get())
        << "Expected field not to be parsed, but it was.";
    return;
  }
  ASSERT_NE(nullptr, field.get());
  field->AddClassificationsForTesting(field_candidates_map_);

  TestClassificationExpectations();
}

// Runs multiple parsing attempts until the end of the form is reached.
void FormFieldParserTestBase::ClassifyAndVerifyWithMultipleParses(
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language) {
  UpdateRanks(fields_);
  ParsingContext context(client_country, page_language,
                         *GetActivePatternFile());
  AutofillScanner scanner(fields_);
  while (!scanner.IsEnd()) {
    // An empty page_language means the language is unknown and patterns of
    // all languages are used.
    std::unique_ptr<FormFieldParser> field = Parse(context, &scanner);
    if (field == nullptr) {
      scanner.Advance();
    } else {
      field->AddClassificationsForTesting(field_candidates_map_);
    }
  }
  TestClassificationExpectations();
}

void FormFieldParserTestBase::TestClassificationExpectations() {
  size_t num_classifications = 0;
  for (const auto [field_id, expected_field_type] : expected_classifications_) {
    FieldType actual_field_type =
        field_candidates_map_.contains(field_id)
            ? field_candidates_map_[field_id].BestHeuristicType()
            : UNKNOWN_TYPE;
    SCOPED_TRACE(testing::Message()
                 << "Found type " << FieldTypeToStringView(actual_field_type)
                 << ", expected type "
                 << FieldTypeToStringView(expected_field_type));
    EXPECT_EQ(expected_field_type, actual_field_type);
    num_classifications += expected_field_type != UNKNOWN_TYPE;
  }
  // There shouldn't be any classifications for other fields.
  EXPECT_EQ(num_classifications, field_candidates_map_.size());
}

FieldRendererId FormFieldParserTestBase::MakeFieldRendererId() {
  return FieldRendererId(++id_counter_);
}

void FormFieldParserTestBase::ClearFieldsAndExpectations() {
  fields_.clear();
  expected_classifications_.clear();
  field_candidates_map_.clear();
}

}  // namespace autofill
