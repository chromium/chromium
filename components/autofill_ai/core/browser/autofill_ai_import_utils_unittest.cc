// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_import_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

using ::autofill::AttributeInstance;
using ::autofill::AttributeType;
using ::autofill::AttributeTypeName;
using ::autofill::AutofillField;
using ::autofill::EntityInstance;
using ::autofill::EntityType;
using ::autofill::FieldType;
using ::autofill::FormControlType;
using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using enum autofill::AttributeTypeName;

std::vector<std::string> GetMonths() {
  static const std::vector<std::string> kMonths = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  return kMonths;
}

// Creates a range containing [min, min+1, ..., max] (both inclusive).
std::vector<std::string> Range(int min, int max) {
  std::vector<std::string> v;
  if (min <= max) {
    v.reserve(max - min + 1);
    while (min <= max) {
      v.push_back(base::NumberToString(min++));
    }
  }
  return v;
}

AttributeInstance CreateAttribute(AttributeTypeName name,
                                  std::string_view value) {
  AttributeType type = AttributeType(name);
  AttributeInstance instance = AttributeInstance(type);
  instance.SetRawInfo(type.field_type(), base::UTF8ToUTF16(value),
                      autofill::VerificationStatus::kObserved);
  instance.FinalizeInfo();
  return instance;
}

void AddPrediction(AutofillField& field, FieldType field_type) {
  autofill::FieldPrediction prediction;
  prediction.set_type(field_type);
  prediction.set_source(
      autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
          FieldPrediction::SOURCE_AUTOFILL_AI);
  field.set_server_predictions({prediction});
}

std::unique_ptr<AutofillField> CreateInput(
    FormControlType form_control_type,
    FieldType field_type,
    std::string_view value,
    std::string_view format_string = "",
    std::string_view initial_value = "") {
  auto field =
      std::make_unique<AutofillField>(autofill::test::CreateTestFormField(
          /*label=*/"",
          /*name=*/"",
          /*value=*/initial_value, form_control_type));
  // Explicitly set the value here to ensure that it differs from the initial
  // value.
  field->set_value(base::UTF8ToUTF16(value));
  if (!format_string.empty()) {
    field->set_format_string_unless_overruled(
        base::UTF8ToUTF16(format_string),
        AutofillField::FormatStringSource::kServer);
  }
  AddPrediction(*field, field_type);
  return field;
}

std::unique_ptr<AutofillField> CreateSelect(std::vector<std::string> values,
                                            std::vector<std::string> texts,
                                            FieldType field_type,
                                            std::string value) {
  values.resize(std::max(values.size(), texts.size()));
  texts.resize(std::max(values.size(), texts.size()));
  auto field =
      std::make_unique<AutofillField>(autofill::test::CreateTestSelectField(
          /*label=*/"", /*name=*/"", /*value=*/value,
          /*autocomplete=*/"",
          /*values=*/base::ToVector(values, &std::string::c_str),
          /*contents=*/base::ToVector(texts, &std::string::c_str)));
  AddPrediction(*field, field_type);
  return field;
}

class AutofillAiImportUtilsTest : public testing::Test {
 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
};

// Tests import that includes and a date distributed over three <input>
// elements.
TEST_F(AutofillAiImportUtilsTest, ImportFromInput) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NAME_TAG,
                               "Karlsson on the Roof"));
  // Input fields for which the initial value is the same as the current value
  // are ignored during import.
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUING_COUNTRY, "Sweden",
                               /*format_string=*/"", "Sweden"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUE_DATE, "24", "DD"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUE_DATE, "12", "MM"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUE_DATE, "2025", "YYYY"));

  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, "en-US"),
              ElementsAre(Property(
                  &EntityInstance::attributes,
                  UnorderedElementsAre(
                      CreateAttribute(kPassportNumber, "123"),
                      CreateAttribute(kPassportName, "Karlsson on the Roof"),
                      CreateAttribute(kPassportIssueDate, "2025-12-24")))));
}

// Tests import that includes a date distributed over three <select> elements.
TEST_F(AutofillAiImportUtilsTest, ImportFromDateSelect) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateSelect(Range(2000, 2099), {},
                                FieldType::PASSPORT_ISSUE_DATE, "2025"));
  fields.push_back(CreateSelect(GetMonths(), {}, FieldType::PASSPORT_ISSUE_DATE,
                                "December"));
  fields.push_back(
      CreateSelect(Range(0, 30), {}, FieldType::PASSPORT_ISSUE_DATE, "23"));

  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, "en-US"),
              ElementsAre(Property(
                  &EntityInstance::attributes,
                  UnorderedElementsAre(
                      CreateAttribute(kPassportNumber, "123"),
                      CreateAttribute(kPassportIssueDate, "2025-12-24")))));
}

// Tests that importing from a non-date <select> element uses the `text` of the
// select option and not its `value`.
TEST_F(AutofillAiImportUtilsTest, ImportFromNonDateSelect) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateSelect(Range(1, 3), {"Germany", "USA", "Vietnam"},
                                FieldType::PASSPORT_ISSUING_COUNTRY, "2"));

  // `CreateAttribute` requires that we use the country code.
  EXPECT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, "en-US"),
      ElementsAre(Property(
          &EntityInstance::attributes,
          UnorderedElementsAre(CreateAttribute(kPassportNumber, "123"),
                               CreateAttribute(kPassportCountry, "US")))));
}

TEST_F(AutofillAiImportUtilsTest, MaybeGetLocalizedDate) {
  using enum AttributeTypeName;
  EntityInstance entity =
      autofill::test::GetPassportEntityInstance({.expiry_date = u"2025-12-30"});
  {
    AttributeInstance a =
        entity.attribute(AttributeType(kPassportExpirationDate)).value();
    auto optional = [](std::u16string s) { return Optional(s); };
    base::i18n::SetICUDefaultLocale("en_US");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"Dec 30, 2025"));
    base::i18n::SetICUDefaultLocale("en_GB");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30 Dec 2025"));
    base::i18n::SetICUDefaultLocale("de_DE");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30. Dez. 2025"));
    base::i18n::SetICUDefaultLocale("fr_FR");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30 déc. 2025"));
  }
  {
    AttributeInstance a =
        entity.attribute(AttributeType(kPassportName)).value();
    EXPECT_EQ(MaybeGetLocalizedDate(a), std::nullopt);
  }
}

}  // namespace
}  // namespace autofill_ai
