// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

class AutofillAiSuggestionsTest : public testing::Test {
 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
};

std::u16string GetEntityInstanceValueForFieldType(
    const autofill::EntityInstance entity,
    autofill::FieldType type) {
  return base::UTF8ToUTF16(
      (entity.attribute(*autofill::AttributeType::FromFieldType(type))
           ->value()));
}

bool AutofillAiPayloadContainsField(
    const autofill::Suggestion::AutofillAiPayload& payload,
    const autofill::AutofillField& field) {
  return payload.values_to_fill.contains(field.global_id());
}

std::u16string AutofillAiPayloadValueForField(
    const autofill::Suggestion::AutofillAiPayload& payload,
    const autofill::AutofillField& field) {
  return payload.values_to_fill.at(field.global_id());
}

std::unique_ptr<autofill::FormStructure> CreateFormStructure(
    const std::vector<autofill::FieldType>& field_types_predictions) {
  autofill::test::FormDescription form_description;
  for (autofill::FieldType field_type : field_types_predictions) {
    form_description.fields.emplace_back(
        autofill::test::FieldDescription({.role = field_type}));
  }
  auto form_structure = std::make_unique<autofill::FormStructure>(
      autofill::test::GetFormData(form_description));
  for (size_t i = 0; i < form_structure->field_count(); i++) {
    autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
        FieldPrediction prediction;
    prediction.set_type(form_description.fields[i].role);
    form_structure->field(i)->set_server_predictions({prediction});
  }
  return form_structure;
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  std::vector<autofill::EntityInstance> entities = {passport_entity};

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PHONE_HOME_WHOLE_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestionsV2(
      *form, form->fields()[0]->global_id(), entities);

  // There should be only one suggestion whose main text matches the entity
  // value for the `triggering_field_type`.
  EXPECT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));

  const autofill::Suggestion::AutofillAiPayload* payload =
      absl::get_if<autofill::Suggestion::AutofillAiPayload>(
          &suggestions[0].payload);
  ASSERT_TRUE(payload);
  // The triggering/first field is of AutofillAi Type.
  EXPECT_TRUE(AutofillAiPayloadContainsField(*payload, *form->fields()[0]));
  EXPECT_EQ(AutofillAiPayloadValueForField(*payload, *form->fields()[0]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  // The second field in the form is also of AutofillAi.
  EXPECT_TRUE(AutofillAiPayloadContainsField(*payload, *form->fields()[1]));
  EXPECT_EQ(AutofillAiPayloadValueForField(*payload, *form->fields()[1]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               autofill::PASSPORT_NUMBER));
  // The third field is not of AutofillAi type.
  EXPECT_FALSE(AutofillAiPayloadContainsField(*payload, *form->fields()[2]));
}

TEST_F(AutofillAiSuggestionsTest,
       GetFillingSuggestion_SkipFieldsThatDoNotMatchTheTriggeringFieldSection) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  std::vector<autofill::EntityInstance> entities = {passport_entity};

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER});
  // Assign different sections to the fields.
  base::flat_map<autofill::LocalFrameToken, size_t> frame_token_ids;
  for (const std::unique_ptr<autofill::AutofillField>& field : form->fields()) {
    field->set_section(
        autofill::Section::FromFieldIdentifier(*field, frame_token_ids));
  }

  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestionsV2(
      *form, form->fields()[0]->global_id(), entities);

  // There should be only one suggestion whose main text matches the entity
  // value for the `triggering_field_type`.
  EXPECT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));

  const autofill::Suggestion::AutofillAiPayload* payload =
      absl::get_if<autofill::Suggestion::AutofillAiPayload>(
          &suggestions[0].payload);
  ASSERT_TRUE(payload);
  // The triggering/first field is of AutofillAi Type.
  EXPECT_TRUE(AutofillAiPayloadContainsField(*payload, *form->fields()[0]));
  EXPECT_EQ(AutofillAiPayloadValueForField(*payload, *form->fields()[0]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  // The second field in the form is of a different section, which means only
  // the first field generated a value to fill.
  EXPECT_EQ((*payload).values_to_fill.size(), 1u);
}

TEST_F(AutofillAiSuggestionsTest, NonMatchingEntity_DoNoReturnSuggestions) {
  autofill::EntityInstance loyalty_card_entity =
      autofill::test::GetLoyaltyCardEntityInstance();
  std::vector<autofill::EntityInstance> entities = {loyalty_card_entity};

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestionsV2(
      *form, form->fields()[0]->global_id(), entities);

  // There should be no suggestion since the triggering is a passport field and
  // the only available entity is for loyalty cards.
  EXPECT_EQ(suggestions.size(), 0u);
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_DedupeSuggestions) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_a_with_different_expiry_date;
  passport_a_with_different_expiry_date.expiry_date = "01/12/2001";
  autofill::test::PassportEntityOptions passport_a_without_an_expiry_date;
  passport_a_with_different_expiry_date.expiry_date = nullptr;
  autofill::test::PassportEntityOptions another_persons_passport_options;
  another_persons_passport_options.name = "Jon doe";
  another_persons_passport_options.number = "927908CYGAS1";
  autofill::EntityInstance another_persons_passport =
      autofill::test::GetPassportEntityInstance(
          another_persons_passport_options);
  std::vector<autofill::EntityInstance> entities = {
      passport_entity, another_persons_passport,
      autofill::test::GetPassportEntityInstance(
          passport_a_with_different_expiry_date),
      autofill::test::GetPassportEntityInstance(
          passport_a_without_an_expiry_date)};

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PASSPORT_ISSUING_COUNTRY_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestionsV2(
      *form, form->fields()[0]->global_id(), entities);

  // The passport with passport_a_with_different_expiry_date should be
  // deduped because while it has an unique attribute (expiry date), the form
  // does not contain a field with autofill::PASSPORT_ISSUE_DATE_TAG, which
  // makes it identical to `passport_entity`. The passport with
  // passport_a_without_an_expiry_date should be deduped because it is a
  // proper subset of `passport_entity`.
  EXPECT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  EXPECT_EQ(suggestions[1].main_text.value,
            GetEntityInstanceValueForFieldType(another_persons_passport,
                                               triggering_field_type));
}

}  // namespace
}  // namespace autofill_ai
