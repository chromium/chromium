// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

std::vector<PatternProviderFeatureState> PatternProviderFeatureState::All() {
  return {
    {.enable = false, .active_source = nullptr},
        {.enable = true, .active_source = "legacy"},
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
        {.enable = true, .active_source = "default"},
        {.enable = true, .active_source = "experimental"},
        {.enable = true, .active_source = "nextgen"},
#endif
  };
}

FormFieldTestBase::FormFieldTestBase(
    PatternProviderFeatureState pattern_provider_feature_state) {
  std::vector<base::test::FeatureRefAndParams> enabled;
  std::vector<base::test::FeatureRef> disabled;
  if (pattern_provider_feature_state.enable) {
    enabled.emplace_back(
        features::kAutofillParsingPatternProvider,
        base::FieldTrialParams{
            {features::kAutofillParsingPatternActiveSource.name,
             pattern_provider_feature_state.active_source}});
  } else {
    disabled.push_back(features::kAutofillParsingPatternProvider);
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
}

FormFieldTestBase::~FormFieldTestBase() = default;

void FormFieldTestBase::AddFormFieldData(FormControlType control_type,
                                         std::string name,
                                         std::string label,
                                         ServerFieldType expected_type) {
  AddFormFieldDataWithLength(control_type, name, label, /*max_length=*/0,
                             expected_type);
}

void FormFieldTestBase::AddFormFieldDataWithLength(
    FormControlType control_type,
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
  AddFormFieldData(FormControlType::kSelectOne, name, label, expected_type);
  FormFieldData* field_data = list_.back().get();
  field_data->options = options;
}

// Convenience wrapper for text control elements.
void FormFieldTestBase::AddTextFormFieldData(std::string name,
                                             std::string label,
                                             ServerFieldType expected_type) {
  AddFormFieldData(FormControlType::kInputText, name, label, expected_type);
}

// Apply parsing and verify the expected types.
// |parsed| indicates if at least one field could be parsed successfully.
// |page_language| the language to be used for parsing, default empty value
// means the language is unknown and patterns of all languages are used.
void FormFieldTestBase::ClassifyAndVerify(
    ParseResult parse_result,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner, client_country, page_language);

  if (parse_result == ParseResult::NOT_PARSED) {
    ASSERT_EQ(nullptr, field_.get());
    return;
  }
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(field_candidates_map_);

  TestClassificationExpectations();
}

void FormFieldTestBase::TestClassificationExpectations() {
  size_t num_classifications = 0;
  for (const auto [field_id, expected_field_type] : expected_classifications_) {
    ServerFieldType actual_field_type =
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

FieldRendererId FormFieldTestBase::MakeFieldRendererId() {
  return FieldRendererId(++id_counter_);
}

void FormFieldTestBase::ClearFieldsAndExpectations() {
  field_ = nullptr;
  list_.clear();
  expected_classifications_.clear();
  field_candidates_map_.clear();
}

}  // namespace autofill
