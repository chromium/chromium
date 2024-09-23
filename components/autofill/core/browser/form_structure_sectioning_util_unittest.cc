// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_sectioning_util.h"

#include <memory>
#include <string>
#include <vector>

#include "autofill_test_utils.h"
#include "base/check_op.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::Bucket;
using base::BucketsAre;

constexpr char kNumberOfSectionsHistogram[] =
    "Autofill.Sectioning.NumberOfSections";
constexpr char kFieldsPerSectionHistogram[] =
    "Autofill.Sectioning.FieldsPerSection";

// The key information from which we build the `FormFieldData` objects for a
// unittest.
struct FieldTemplate {
  FieldType field_type = UNKNOWN_TYPE;
  FormControlType form_control_type = FormControlType::kInputText;
  std::string autocomplete_section = "";
  HtmlFieldMode autocomplete_mode = HtmlFieldMode::kNone;
  bool is_focusable = true;
};

// Returns fields to be sectioned.
std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const std::vector<FieldTemplate>& field_templates) {
  std::vector<std::unique_ptr<AutofillField>> result;
  result.reserve(field_templates.size());
  for (const auto& t : field_templates) {
    const auto& f =
        result.emplace_back(std::make_unique<AutofillField>(FormFieldData()));
    f->set_renderer_id(test::MakeFieldRendererId());
    f->set_form_control_type(t.form_control_type);
    f->SetTypeTo(AutofillType(t.field_type));
    DCHECK_EQ(f->Type().GetStorableType(), t.field_type);
    if (!t.autocomplete_section.empty() ||
        t.autocomplete_mode != HtmlFieldMode::kNone) {
      f->set_parsed_autocomplete(AutocompleteParsingResult{
          .section = t.autocomplete_section, .mode = t.autocomplete_mode});
    }
    f->set_is_focusable(t.is_focusable);
  }
  return result;
}

std::vector<Section> GetSections(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  std::vector<Section> sections;
  sections.reserve(fields.size());
  for (const auto& field : fields)
    sections.push_back(field->section());
  return sections;
}

class FormStructureSectioningTest : public testing::Test {
 public:
  void AssignSectionsAndLogMetrics(
      base::span<const std::unique_ptr<AutofillField>> fields) {
    AssignSections(fields);
    // Since only the UMA metrics are tested, the form signature and UKM logger
    // are irrelevant.
    LogSectioningMetrics(FormSignature(0UL), fields,
                         /*form_interactions_ukm_logger=*/nullptr);
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// This refers to the example from the code comment in form_sectioning_util.h.
std::vector<std::unique_ptr<AutofillField>> CreateExampleFields() {
  return CreateFields(
      {{.field_type = NAME_FULL},
       {.field_type = ADDRESS_HOME_COUNTRY},
       {.field_type = NAME_FULL, .autocomplete_section = "A"},
       {.field_type = ADDRESS_HOME_STREET_NAME},
       {.field_type = CREDIT_CARD_NUMBER},
       {.field_type = CREDIT_CARD_NUMBER, .is_focusable = false},
       {.field_type = NAME_FULL},
       {.field_type = ADDRESS_HOME_COUNTRY},
       {.field_type = CREDIT_CARD_NUMBER}});
}

TEST_F(FormStructureSectioningTest, ExampleFormNoSectioningMode) {
  auto fields = CreateExampleFields();
  base::HistogramTester histogram_tester;
  AssignSectionsAndLogMetrics(fields);

  // The evaluation order of the `Section::FromFieldIdentifier()` expressions
  // does not matter, as all `FormFieldData::host_frame` are identical.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  EXPECT_THAT(GetSections(fields),
              testing::ElementsAre(
                  Section::FromFieldIdentifier(*fields[0], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[0], frame_token_ids),
                  Section::FromAutocomplete(
                      {.section = fields[2]->parsed_autocomplete()->section}),
                  Section::FromFieldIdentifier(*fields[0], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[6], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[6], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids)));
  // The metrics ignore the section of field #5 because it's unfocusable.
  EXPECT_EQ(ComputeSectioningSignature(fields), StrToHash32Bit("00102332"));
  histogram_tester.ExpectUniqueSample(kNumberOfSectionsHistogram, 4, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kFieldsPerSectionHistogram),
              BucketsAre(Bucket(1, 1), Bucket(2, 2), Bucket(3, 1)));
}

// Tests that an invisible <select> does not start a new section. Consider the
// following form:
//   <form>
//     <div style="display: none">
//       Name: <input>
//       Country: <select>...</select>
//       ...
//     </div>
//     <div style="display: block">
//       Name: <input>
//       Country: <select>...</select>
//       ...
//     </div>
//   </form>
// The fields from the first <div> must be in a different section than the
// fields in the second <div>. In particular, the first <select> (even though it
// *is* sectionable) must not start a section to which the name <input> is then
// added.
TEST_F(FormStructureSectioningTest,
       SelectFieldOfHiddenSectionDoesNotLeakIntoFollowingSection) {
  auto fields =
      CreateFields({{.field_type = NAME_FULL, .is_focusable = false},
                    {.field_type = ADDRESS_HOME_LINE1, .is_focusable = false},
                    {.field_type = ADDRESS_HOME_LINE2, .is_focusable = false},
                    {.field_type = ADDRESS_HOME_COUNTRY,
                     .form_control_type = FormControlType::kSelectOne,
                     .is_focusable = false},
                    {.field_type = NAME_FULL},
                    {.field_type = ADDRESS_HOME_LINE1},
                    {.field_type = ADDRESS_HOME_LINE2},
                    {.field_type = ADDRESS_HOME_COUNTRY,
                     .form_control_type = FormControlType::kSelectOne}});

  base::HistogramTester histogram_tester;
  AssignSectionsAndLogMetrics(fields);

  // The evaluation order of the `Section::FromFieldIdentifier()` expressions
  // does not matter, as all `FormFieldData::host_frame` are identical.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  EXPECT_THAT(GetSections(fields),
              testing::ElementsAre(
                  Section(), Section(), Section(), Section(),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids),
                  Section::FromFieldIdentifier(*fields[4], frame_token_ids)));
  EXPECT_EQ(ComputeSectioningSignature(fields), StrToHash32Bit("01111"));
  histogram_tester.ExpectUniqueSample(kNumberOfSectionsHistogram, 2, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kFieldsPerSectionHistogram),
              BucketsAre(Bucket(1, 1), Bucket(4, 1)));
}

// Tests that repeated sequences of state and country do not start a new
// section. Consider the following form:
//   <form>
//     Name: <input>
//     Address line 1: <input>
//     Address line 2: <input>
//     <div style="display: block">
//       State: <select>...</select>
//       Country: <select>...</select>
//     </div>
//     <div style="display: none">
//       State: <select>...</select>
//       Country: <select>...</select>
//     </div>
//     Phone: <input>
//   </form>
// The fields in the second <div> should not start a new section.
TEST_F(FormStructureSectioningTest,
       RepeatedSequenceOfStateCountryEtcDoesNotBreakSection) {
  auto fields = CreateFields({{.field_type = NAME_FULL},
                              {.field_type = ADDRESS_HOME_LINE1},
                              {.field_type = ADDRESS_HOME_LINE2},
                              {.field_type = ADDRESS_HOME_STATE,
                               .form_control_type = FormControlType::kSelectOne,
                               .is_focusable = true},
                              {.field_type = ADDRESS_HOME_COUNTRY,
                               .form_control_type = FormControlType::kSelectOne,
                               .is_focusable = true},
                              {.field_type = ADDRESS_HOME_STATE,
                               .form_control_type = FormControlType::kSelectOne,
                               .is_focusable = false},
                              {.field_type = ADDRESS_HOME_COUNTRY,
                               .form_control_type = FormControlType::kSelectOne,
                               .is_focusable = false},
                              {.field_type = PHONE_HOME_WHOLE_NUMBER}});

  base::HistogramTester histogram_tester;
  AssignSectionsAndLogMetrics(fields);

  // The evaluation order of the `Section::FromFieldIdentifier()` expressions
  // does not matter, as all `FormFieldData::host_frame` are identical.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  EXPECT_THAT(GetSections(fields), testing::Each(Section::FromFieldIdentifier(
                                       *fields[0], frame_token_ids)));
  EXPECT_EQ(ComputeSectioningSignature(fields), StrToHash32Bit("00000000"));
  histogram_tester.ExpectUniqueSample(kNumberOfSectionsHistogram, 1, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kFieldsPerSectionHistogram),
              BucketsAre(Bucket(8, 1)));
}

}  // namespace
}  // namespace autofill
