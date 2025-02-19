// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <ranges>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {

namespace {

using autofill::Suggestion;
using autofill::SuggestionType;
using ::testing::Ge;
using ::testing::SizeIs;

class AutofillAiSuggestionsTest : public testing::Test {
 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
};

size_t CountFillingSuggestions(base::span<const Suggestion> suggestions) {
  return std::ranges::count_if(suggestions, [](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kFillAutofillAi;
  });
}

std::u16string GetEntityInstanceValueForFieldType(
    const autofill::EntityInstance entity,
    autofill::FieldType type) {
  return entity.attribute(*autofill::AttributeType::FromFieldType(type))
      ->value();
}

bool AutofillAiPayloadContainsField(
    const Suggestion::AutofillAiPayload& payload,
    const autofill::AutofillField& field) {
  return payload.values_to_fill.contains(field.global_id());
}

std::u16string AutofillAiPayloadValueForField(
    const Suggestion::AutofillAiPayload& payload,
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

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PassportEntity) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  std::vector<autofill::EntityInstance> entities = {passport_entity};

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PHONE_HOME_WHOLE_NUMBER});
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(), entities);

  // There should be only one suggestion whose main text matches the entity
  // value for the `triggering_field_type`.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  EXPECT_EQ(suggestions[1].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kManageAutofillAi);

  const Suggestion::AutofillAiPayload* payload =
      absl::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(suggestions[0].icon, autofill::Suggestion::Icon::kIdCard);

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

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PrefixMatching) {
  autofill::test::PassportEntityOptions passport_prefix_matches_options;
  passport_prefix_matches_options.name = u"Jon Doe";
  autofill::EntityInstance passport_prefix_matches =
      autofill::test::GetPassportEntityInstance(
          passport_prefix_matches_options);

  autofill::test::PassportEntityOptions passport_prefix_does_not_match_options;
  passport_prefix_does_not_match_options.name = u"Harry Potter";
  autofill::EntityInstance passport_prefix_does_not_match =
      autofill::test::GetPassportEntityInstance(
          passport_prefix_does_not_match_options);

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PHONE_HOME_WHOLE_NUMBER});

  form->field(0)->set_value(u"J");

  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_prefix_matches, passport_prefix_does_not_match});

  // There should be only one suggestion whose main text matches is a prefix of
  // the value already existing in the triggering field.
  // Note that there is one separator and one footer suggestion as well.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_prefix_matches,
                                               triggering_field_type));
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_LoyaltyCardEntity) {
  autofill::EntityInstance loyalty_card_entity =
      autofill::test::GetLoyaltyCardEntityInstance();
  std::vector<autofill::EntityInstance> entities = {loyalty_card_entity};

  autofill::FieldType triggering_field_type = autofill::LOYALTY_MEMBERSHIP_ID;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::LOYALTY_MEMBERSHIP_PROVIDER,
       autofill::EMAIL_ADDRESS});
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(), entities);

  // There should be only one suggestion whose main text matches the entity
  // value for the `triggering_field_type`.
  EXPECT_THAT(suggestions, SizeIs(Ge(1)));
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(loyalty_card_entity,
                                               triggering_field_type));

  const autofill::Suggestion::AutofillAiPayload* payload =
      absl::get_if<autofill::Suggestion::AutofillAiPayload>(
          &suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(suggestions[0].icon, autofill::Suggestion::Icon::kLoyalty);

  // The triggering/first field is of AutofillAi Type.
  EXPECT_TRUE(AutofillAiPayloadContainsField(*payload, *form->fields()[0]));
  EXPECT_EQ(AutofillAiPayloadValueForField(*payload, *form->fields()[0]),
            GetEntityInstanceValueForFieldType(loyalty_card_entity,
                                               triggering_field_type));
  // The second field in the form is also of AutofillAi.
  EXPECT_TRUE(AutofillAiPayloadContainsField(*payload, *form->fields()[1]));
  EXPECT_EQ(AutofillAiPayloadValueForField(*payload, *form->fields()[1]),
            GetEntityInstanceValueForFieldType(
                loyalty_card_entity, autofill::LOYALTY_MEMBERSHIP_PROVIDER));
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

  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(), entities);

  // There should be only one suggestion whose main text matches the entity
  // value for the `triggering_field_type`.
  EXPECT_THAT(suggestions, SizeIs(Ge(1)));
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));

  const Suggestion::AutofillAiPayload* payload =
      absl::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
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
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(), entities);

  // There should be no suggestion since the triggering is a passport field and
  // the only available entity is for loyalty cards.
  EXPECT_EQ(suggestions.size(), 0u);
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_DedupeSuggestions) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_a_with_different_expiry_date;
  passport_a_with_different_expiry_date.expiry_date = u"01/12/2001";
  autofill::test::PassportEntityOptions passport_a_without_an_expiry_date;
  passport_a_with_different_expiry_date.expiry_date = nullptr;
  autofill::test::PassportEntityOptions another_persons_passport_options;
  another_persons_passport_options.name = u"Jon doe";
  another_persons_passport_options.number = u"927908CYGAS1";
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
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(), entities);

  // The passport with passport_a_with_different_expiry_date should be
  // deduped because while it has an unique attribute (expiry date), the form
  // does not contain a field with autofill::PASSPORT_ISSUE_DATE_TAG, which
  // makes it identical to `passport_entity`. The passport with
  // passport_a_without_an_expiry_date should be deduped because it is a
  // proper subset of `passport_entity`.
  EXPECT_THAT(suggestions, SizeIs(Ge(2)));
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  EXPECT_EQ(suggestions[1].main_text.value,
            GetEntityInstanceValueForFieldType(another_persons_passport,
                                               triggering_field_type));
}

// Tests that an "Undo Autofill" suggestion is appended if the trigger field
// is autofilled.
TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestions_Undo) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();

  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity});

  EXPECT_FALSE(base::Contains(
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity}),
      SuggestionType::kUndoOrClear, &Suggestion::type));

  form->field(0)->set_is_autofilled(true);
  EXPECT_TRUE(base::Contains(
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity}),
      SuggestionType::kUndoOrClear, &Suggestion::type));
}

TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_SingleSuggestion_OneFieldFilled) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 1u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport");
}

TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_SingleSuggestion_TwoFieldsFilled) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 1u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · 123");
}

// Note that in this test the passport country is used in the label, even though
// the form also contains expiry date field. This is because country data
// appears before expiry date in the disambiguation order for passport entities.
TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_SingleSuggestion_TwoFieldsFilled_UseFieldPriorityOrder) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 1u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Sweden");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentCountries_AddDifferentiatingLabel) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_entity_b_options;
  passport_entity_b_options.country = u"Brazil";
  autofill::EntityInstance passport_entity_b =
      autofill::test::GetPassportEntityInstance(passport_entity_b_options);

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity, passport_entity_b});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 2u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Sweden");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport · Brazil");
}

TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_ThreeSuggestions_AddDifferentiatingLabel) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_entity_b_options;
  passport_entity_b_options.country = u"Brazil";
  autofill::EntityInstance passport_entity_b =
      autofill::test::GetPassportEntityInstance(passport_entity_b_options);
  autofill::test::PassportEntityOptions passport_entity_c_options;
  passport_entity_c_options.expiry_date = u"12/2018";
  autofill::EntityInstance passport_entity_c =
      autofill::test::GetPassportEntityInstance(passport_entity_c_options);

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  // Note that `autofill::PASSPORT_ISSUING_COUNTRY_TAG` appears twice in the
  // form, yet due to deduping it only adds its equivalent label once.
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_ISSUING_COUNTRY_TAG, autofill::PASSPORT_NUMBER,
       autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity, passport_entity_b, passport_entity_c});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 3u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Sweden · 12/2019");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport · Brazil · 12/2019");

  EXPECT_EQ(suggestions[2].labels.size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0][0].value, u"Passport · Sweden · 12/2018");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_ThreeSuggestions_WithMissingValues_AddDifferentiatingLabel) {
  autofill::test::PassportEntityOptions passport_entity_a_options;
  passport_entity_a_options.country = u"Brazil";
  autofill::EntityInstance passport_entity_a =
      autofill::test::GetPassportEntityInstance(passport_entity_a_options);

  autofill::test::PassportEntityOptions passport_entity_b_options;
  // Note that passport b can only fill the triggering name field and has no
  // country data label to add.
  passport_entity_b_options.name = u"Jack Sparrow";
  passport_entity_b_options.country = nullptr;
  autofill::EntityInstance passport_entity_b =
      autofill::test::GetPassportEntityInstance(passport_entity_b_options);

  autofill::EntityInstance passport_entity_c =
      autofill::test::GetPassportEntityInstance();

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity_a, passport_entity_b, passport_entity_c});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 3u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Brazil");

  // The second suggestion can only fill the triggering field and can add no
  // other label.
  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport");

  EXPECT_EQ(suggestions[2].labels.size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0][0].value, u"Passport · Sweden");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentExpiryDates_AddDifferentiatingLabel) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_entity_b_options;
  passport_entity_b_options.expiry_date = u"12/2018";
  autofill::EntityInstance passport_entity_b =
      autofill::test::GetPassportEntityInstance(passport_entity_b_options);

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER, autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity, passport_entity_b});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 2u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · 12/2019");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport · 12/2018");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_DifferentMainText_NoDifferentiatingLabel) {
  autofill::EntityInstance passport_entity =
      autofill::test::GetPassportEntityInstance();
  autofill::test::PassportEntityOptions passport_entity_b_options;
  passport_entity_b_options.name = u"Lebowski";
  autofill::EntityInstance passport_entity_b =
      autofill::test::GetPassportEntityInstance(passport_entity_b_options);

  autofill::FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions =
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity, passport_entity_b});

  ASSERT_EQ(CountFillingSuggestions(suggestions), 2u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Sweden");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport · Sweden");
}

}  // namespace
}  // namespace autofill_ai
