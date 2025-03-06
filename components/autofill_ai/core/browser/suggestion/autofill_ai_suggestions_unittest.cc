// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <ranges>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {

namespace {

using autofill::AutofillField;
using autofill::FieldType;
using autofill::FormStructure;
using autofill::Suggestion;
using autofill::SuggestionType;
using FieldPrediction = autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

constexpr char kAppLocaleUS[] = "en-US";

autofill::EntityInstance MakePassportWithRandomGuid(
    autofill::test::PassportEntityOptions options = {}) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return autofill::test::GetPassportEntityInstance(options);
}

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
    FieldType type,
    const std::string& app_locale = kAppLocaleUS) {
  return entity.attribute(*autofill::AttributeType::FromFieldType(type))
      ->GetInfo(type, app_locale, /*format_string=*/std::nullopt);
}

std::optional<std::u16string> GetFillValueForField(
    base::span<const autofill::EntityInstance> entities,
    const Suggestion::AutofillAiPayload& payload,
    const AutofillField& field,
    const std::string& app_locale = kAppLocaleUS) {
  auto entity_it = std::ranges::find(entities, payload.guid,
                                     &autofill::EntityInstance::guid);
  if (entity_it == entities.end()) {
    return std::nullopt;
  }
  auto attribute_it = std::ranges::find_if(
      entity_it->attributes(),
      [&field](const autofill::AttributeInstance& attribute) {
        return attribute.type().field_type() ==
               field.GetAutofillAiServerTypePredictions();
      });
  if (attribute_it == entity_it->attributes().end()) {
    return std::nullopt;
  }
  return attribute_it->GetInfo(field.Type().GetStorableType(), app_locale,
                               field.format_string());
}

std::unique_ptr<FormStructure> CreateFormStructureWithMultiplePredictions(
    const std::vector<std::vector<FieldType>>& multiple_field_types) {
  autofill::test::FormDescription form_description;
  for (std::vector<FieldType> field_types : multiple_field_types) {
    FieldType type =
        field_types.empty() ? autofill::UNKNOWN_TYPE : field_types[0];
    form_description.fields.emplace_back(
        autofill::test::FieldDescription({.role = type}));
  }
  auto form_structure = std::make_unique<FormStructure>(
      autofill::test::GetFormData(form_description));
  CHECK_EQ(multiple_field_types.size(), form_structure->field_count());
  for (size_t i = 0; i < form_structure->field_count(); i++) {
    form_structure->field(i)->set_server_predictions(
        base::ToVector(multiple_field_types[i], [](FieldType type) {
          FieldPrediction prediction;
          prediction.set_type(type);
          return prediction;
        }));
  }
  return form_structure;
}

std::unique_ptr<FormStructure> CreateFormStructure(
    const std::vector<FieldType>& field_types) {
  return CreateFormStructureWithMultiplePredictions(base::ToVector(
      field_types,
      [](FieldType type) { return std::vector<FieldType>({type}); }));
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PassportEntity) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  std::vector<autofill::EntityInstance> entities = {passport_entity};

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PHONE_HOME_WHOLE_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), entities, kAppLocaleUS);

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
  EXPECT_EQ(GetFillValueForField(entities, *payload, *form->fields()[0]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
  // The second field in the form is also of AutofillAi.
  EXPECT_EQ(GetFillValueForField(entities, *payload, *form->fields()[1]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               autofill::PASSPORT_NUMBER));
  // The third field is not of AutofillAi type.
  EXPECT_EQ(GetFillValueForField(entities, *payload, *form->fields()[2]),
            std::nullopt);
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PrefixMatching) {
  autofill::EntityInstance passport_prefix_matches =
      MakePassportWithRandomGuid({.name = u"Jon Doe"});

  autofill::EntityInstance passport_prefix_does_not_match =
      MakePassportWithRandomGuid({.name = u"Harry Potter"});

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PHONE_HOME_WHOLE_NUMBER});

  form->field(0)->set_value(u"J");

  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_prefix_matches, passport_prefix_does_not_match}, kAppLocaleUS);

  // There should be only one suggestion whose main text matches is a prefix of
  // the value already existing in the triggering field.
  // Note that there is one separator and one footer suggestion as well.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(passport_prefix_matches,
                                               triggering_field_type));
}

// Tests that no prefix matching is performed if the attribute that would be
// filled into the triggering field is obfuscated.
TEST_F(AutofillAiSuggestionsTest,
       GetFillingSuggestionNoPrefixMatchingForObfuscatedAttributes) {
  autofill::EntityInstance passport =
      MakePassportWithRandomGuid({.number = u"12345"});

  FieldType triggering_field_type = autofill::PASSPORT_NUMBER;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG});

  form->field(0)->set_value(u"12");

  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport}, kAppLocaleUS);
  EXPECT_FALSE(suggestions.empty());
}

TEST_F(AutofillAiSuggestionsTest,
       GetFillingSuggestion_SkipFieldsThatDoNotMatchTheTriggeringFieldSection) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  std::vector<autofill::EntityInstance> entities = {passport_entity};

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER});
  // Assign different sections to the fields.
  base::flat_map<autofill::LocalFrameToken, size_t> frame_token_ids;
  for (const std::unique_ptr<AutofillField>& field : form->fields()) {
    field->set_section(
        autofill::Section::FromFieldIdentifier(*field, frame_token_ids));
  }

  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), entities, kAppLocaleUS);

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
  EXPECT_EQ(GetFillValueForField(entities, *payload, *form->fields()[0]),
            GetEntityInstanceValueForFieldType(passport_entity,
                                               triggering_field_type));
}

TEST_F(AutofillAiSuggestionsTest, NonMatchingEntity_DoNoReturnSuggestions) {
  autofill::EntityInstance drivers_license_entity =
      autofill::test::GetDriversLicenseEntityInstance();
  std::vector<autofill::EntityInstance> entities = {drivers_license_entity};

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), entities, kAppLocaleUS);

  // There should be no suggestion since the triggering is a passport field and
  // the only available entity is for loyalty cards.
  EXPECT_EQ(suggestions.size(), 0u);
}

// Tests that suggestions whose structured attribute would have empty text for
// the value to fill into the triggering field are not shown.
TEST_F(AutofillAiSuggestionsTest, EmptyMainTextForStructuredAttribute) {
  autofill::EntityInstance passport =
      MakePassportWithRandomGuid({.name = u"Miller"});

  std::unique_ptr<FormStructure> form =
      CreateFormStructureWithMultiplePredictions(
          {{autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG},
           {autofill::NAME_LAST, autofill::PASSPORT_NAME_TAG},
           {autofill::PASSPORT_NUMBER}});

  base::optional_ref<const autofill::AttributeInstance> name_attribute =
      passport.attribute(
          autofill::AttributeType(autofill::AttributeTypeName::kPassportName));
  ASSERT_TRUE(name_attribute);
  ASSERT_THAT(
      name_attribute->GetInfo(autofill::NAME_FIRST, kAppLocaleUS, std::nullopt),
      u"");
  ASSERT_THAT(
      name_attribute->GetInfo(autofill::NAME_LAST, kAppLocaleUS, std::nullopt),
      u"Miller");

  EXPECT_THAT(CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                                       {passport}, kAppLocaleUS),
              IsEmpty());
  EXPECT_THAT(CreateFillingSuggestions(*form, form->fields()[1]->global_id(),
                                       {passport}, kAppLocaleUS),
              Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_DedupeSuggestions) {
  autofill::EntityInstance passport = MakePassportWithRandomGuid();

  autofill::EntityInstance passport_a_with_different_expiry_date =
      MakePassportWithRandomGuid({.expiry_date = u"2001-12-01"});

  autofill::EntityInstance passport_a_without_an_expiry_date =
      MakePassportWithRandomGuid({.expiry_date = nullptr});

  autofill::EntityInstance another_persons_passport =
      MakePassportWithRandomGuid(
          {.name = u"Jon doe", .number = u"927908CYGAS1"});

  std::vector<autofill::EntityInstance> entities = {
      passport, another_persons_passport, passport_a_with_different_expiry_date,
      passport_a_without_an_expiry_date};

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER,
                           autofill::PASSPORT_ISSUING_COUNTRY_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), entities, kAppLocaleUS);

  // The passport with passport_a_with_different_expiry_date should be
  // deduped because while it has an unique attribute (expiry date), the form
  // does not contain a field with autofill::PASSPORT_ISSUE_DATE_TAG, which
  // makes it identical to `passport`. The passport with
  // passport_a_without_an_expiry_date should be deduped because it is a
  // proper subset of `passport`.
  ASSERT_THAT(suggestions, SizeIs(Ge(2)));
  EXPECT_EQ(suggestions[0].main_text.value,
            GetEntityInstanceValueForFieldType(another_persons_passport,
                                               triggering_field_type));
  EXPECT_EQ(
      suggestions[1].main_text.value,
      GetEntityInstanceValueForFieldType(passport, triggering_field_type));
}

// Tests that an "Undo Autofill" suggestion is appended if the trigger field
// is autofilled.
TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestions_Undo) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();

  std::unique_ptr<FormStructure> form =
      CreateFormStructure({autofill::PASSPORT_NUMBER});

  EXPECT_FALSE(base::Contains(
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity}, kAppLocaleUS),
      SuggestionType::kUndoOrClear, &Suggestion::type));

  form->field(0)->set_is_autofilled(true);
  EXPECT_TRUE(base::Contains(
      CreateFillingSuggestions(*form, form->fields()[0]->global_id(),
                               {passport_entity}, kAppLocaleUS),
      SuggestionType::kUndoOrClear, &Suggestion::type));
}

TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_SingleSuggestion_OneFieldFilled) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity}, kAppLocaleUS);

  ASSERT_EQ(CountFillingSuggestions(suggestions), 1u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport");
}

TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_SingleSuggestion_TwoFieldsFilled) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({triggering_field_type, autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity}, kAppLocaleUS);

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
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(), {passport_entity}, kAppLocaleUS);

  ASSERT_EQ(CountFillingSuggestions(suggestions), 1u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · Sweden");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentCountries_AddDifferentiatingLabel) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  autofill::EntityInstance passport_entity_b =
      MakePassportWithRandomGuid({.country = u"Brazil"});

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity, passport_entity_b}, kAppLocaleUS);

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
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  autofill::EntityInstance passport_entity_b =
      MakePassportWithRandomGuid({.country = u"Brazil"});
  autofill::EntityInstance passport_entity_c =
      MakePassportWithRandomGuid({.expiry_date = u"2018-12-31"});

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  // Note that `autofill::PASSPORT_ISSUING_COUNTRY_TAG` appears twice in the
  // form, yet due to deduping it only adds its equivalent label once.
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_ISSUING_COUNTRY_TAG, autofill::PASSPORT_NUMBER,
       autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity, passport_entity_b, passport_entity_c}, kAppLocaleUS);

  ASSERT_EQ(CountFillingSuggestions(suggestions), 3u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value,
            u"Passport · Sweden · 2019-08-30");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value,
            u"Passport · Brazil · 2019-08-30");

  EXPECT_EQ(suggestions[2].labels.size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[2].labels[0][0].value,
            u"Passport · Sweden · 2018-12-31");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_ThreeSuggestions_WithMissingValues_AddDifferentiatingLabel) {
  autofill::EntityInstance passport_entity_a =
      MakePassportWithRandomGuid({.country = u"Brazil"});

  // Note that passport b can only fill the triggering name field and has no
  // country data label to add.
  autofill::EntityInstance passport_entity_b =
      MakePassportWithRandomGuid({.name = u"Jack Sparrow", .country = nullptr});

  autofill::EntityInstance passport_entity_c = MakePassportWithRandomGuid();

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity_a, passport_entity_b, passport_entity_c}, kAppLocaleUS);

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
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  autofill::EntityInstance passport_entity_b =
      MakePassportWithRandomGuid({.expiry_date = u"2018-12-29"});

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER, autofill::PASSPORT_EXPIRATION_DATE_TAG});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity, passport_entity_b}, kAppLocaleUS);

  ASSERT_EQ(CountFillingSuggestions(suggestions), 2u);
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value, u"Passport · 2019-08-30");

  EXPECT_EQ(suggestions[1].labels.size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[0][0].value, u"Passport · 2018-12-29");
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_DifferentMainText_NoDifferentiatingLabel) {
  autofill::EntityInstance passport_entity = MakePassportWithRandomGuid();
  autofill::EntityInstance passport_entity_b =
      MakePassportWithRandomGuid({.name = u"Lebowski"});

  FieldType triggering_field_type = autofill::PASSPORT_NAME_TAG;
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {triggering_field_type, autofill::PASSPORT_ISSUING_COUNTRY_TAG,
       autofill::PASSPORT_NUMBER});
  std::vector<autofill::Suggestion> suggestions = CreateFillingSuggestions(
      *form, form->fields()[0]->global_id(),
      {passport_entity, passport_entity_b}, kAppLocaleUS);

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
