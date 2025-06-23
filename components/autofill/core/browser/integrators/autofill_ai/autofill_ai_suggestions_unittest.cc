// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_suggestions.h"

#include <optional>
#include <ranges>
#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::ResultOf;

constexpr char kAppLocaleUS[] = "en-US";

EntityInstance MakePassportWithRandomGuid(
    test::PassportEntityOptions options = {}) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return test::GetPassportEntityInstance(options);
}

EntityInstance MakeVehicleWithRandomGuid(test::VehicleOptions options = {}) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return test::GetVehicleEntityInstance(options);
}

Matcher<const Suggestion&> HasMainText(const std::u16string& text) {
  return ResultOf(
      "Suggestion::main_text.value",
      [](const Suggestion& s) { return s.main_text.value; }, text);
}

Matcher<const Suggestion&> HasLabel(const std::u16string& label) {
  return Field(
      &Suggestion::labels,
      ElementsAre(ElementsAre(Field(&Suggestion::Text::value, label))));
}

Matcher<const Suggestion&> HasType(SuggestionType type) {
  return Field("Suggestion::type", &Suggestion::type, type);
}

auto SuggestionsAre(auto&&... matchers) {
  return ElementsAre(std::forward<decltype(matchers)>(matchers)...,
                     HasType(SuggestionType::kSeparator),
                     HasType(SuggestionType::kManageAutofillAi));
}

class AutofillAiSuggestionsTest : public testing::Test {
 public:
  void SetEntities(std::vector<EntityInstance> entities) {
    entities_ = std::move(entities);
  }

  // Sets the form to one whose `i`th field has type `field_types[i]`.
  void SetForm(const std::vector<FieldType>& field_types) {
    test::FormDescription form_description;
    for (FieldType type : field_types) {
      form_description.fields.emplace_back(
          test::FieldDescription({.role = type}));
    }
    form_structure_.emplace(test::GetFormData(form_description));
    CHECK_EQ(field_types.size(), form_structure_->field_count());
    for (size_t i = 0; i < form_structure_->field_count(); i++) {
      form_structure_->field(i)->set_server_predictions({[&] {
        FieldPrediction prediction;
        prediction.set_type(field_types[i]);
        return prediction;
      }()});
    }
  }

  AutofillField& field(size_t i) { return *form_structure_->fields()[i]; }

  std::optional<std::u16string> GetFillValueForField(
      const Suggestion::AutofillAiPayload& payload,
      const AutofillField& field) {
    auto it = std::ranges::find(entities_, payload.guid, &EntityInstance::guid);
    if (it == entities_.end()) {
      return std::nullopt;
    }
    const EntityInstance& entity = *it;

    std::vector<AutofillFieldWithAttributeType> fields_and_types =
        DetermineAttributeTypes(*form_structure_, field.section(),
                                entity.type());
    auto jt = std::ranges::find(fields_and_types, field.global_id(),
                                [](const AutofillFieldWithAttributeType& f) {
                                  return f.field->global_id();
                                });
    if (jt == fields_and_types.end()) {
      return std::nullopt;
    }
    const AttributeType type = jt->type;

    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(type);
    if (!attribute) {
      return std::nullopt;
    }
    return attribute->GetInfo(field.Type().GetStorableType(), kAppLocaleUS,
                              field.format_string());
  }

  std::vector<Suggestion> CreateFillingSuggestions(const AutofillField& field) {
    return autofill::CreateFillingSuggestions(*form_structure_, field,
                                              entities_, kAppLocaleUS);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiNoTagTypes};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::vector<EntityInstance> entities_;
  std::optional<FormStructure> form_structure_;
};

std::u16string GetPassportName(const EntityInstance& entity) {
  return entity.attribute(AttributeType(AttributeTypeName::kPassportName))
      ->GetCompleteInfo(kAppLocaleUS);
}

std::u16string GetPassportNumber(const EntityInstance& entity) {
  return entity.attribute(AttributeType(AttributeTypeName::kPassportNumber))
      ->GetCompleteInfo(kAppLocaleUS);
}

// Tests that no suggestions are generated when the field has a non-Autofill AI
// type.
TEST_F(AutofillAiSuggestionsTest, NoSuggestionsOnNonAiField) {
  SetEntities({MakePassportWithRandomGuid()});
  SetForm({ADDRESS_HOME_ZIP, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  EXPECT_THAT(CreateFillingSuggestions(field(0)), IsEmpty());
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PassportEntity) {
  EntityInstance passport_entity = MakePassportWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});

  std::vector<Suggestion> suggestions = CreateFillingSuggestions(field(0));

  // There should be only one suggestion whose main text matches the entity
  // value for the passport name.
  EXPECT_THAT(suggestions,
              SuggestionsAre(HasMainText(GetPassportName(passport_entity))));

  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kIdCard));

  // The triggering/first field is of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(0)),
            GetPassportName(passport_entity));
  // The second field in the form is also of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(1)),
            GetPassportNumber(passport_entity));
  // The third field is not of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(2)), std::nullopt);
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_PrefixMatching) {
  EntityInstance passport_prefix_matches =
      MakePassportWithRandomGuid({.name = u"Jon Doe"});
  EntityInstance passport_prefix_does_not_match =
      MakePassportWithRandomGuid({.name = u"Harry Potter"});

  SetEntities({passport_prefix_matches, passport_prefix_does_not_match});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  field(0).set_value(u"J");

  // There should be only one suggestion whose main text matches is a prefix of
  // the value already existing in the triggering field.
  // Note that there is one separator and one footer suggestion as well.
  EXPECT_THAT(
      CreateFillingSuggestions(field(0)),
      SuggestionsAre(HasMainText(GetPassportName(passport_prefix_matches))));
}

// Tests that no prefix matching is performed if the attribute that would be
// filled into the triggering field is obfuscated.
TEST_F(AutofillAiSuggestionsTest,
       GetFillingSuggestionNoPrefixMatchingForObfuscatedAttributes) {
  SetEntities({MakePassportWithRandomGuid({.number = u"12345"})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  field(0).set_value(u"12");
  EXPECT_THAT(CreateFillingSuggestions(field(0)), Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionsTest,
       GetFillingSuggestion_SkipFieldsThatDoNotMatchTheTriggeringFieldSection) {
  EntityInstance passport_entity = MakePassportWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE});

  field(0).set_section(Section::FromAutocomplete(Section::Autocomplete("foo")));
  field(1).set_section(Section::FromAutocomplete(Section::Autocomplete("bar")));
  ASSERT_NE(field(0).section(), field(1).section());

  std::vector<Suggestion> suggestions = CreateFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              SuggestionsAre(HasMainText(GetPassportNumber(passport_entity))));
  EXPECT_THAT(suggestions, SuggestionsAre(HasLabel(u"Passport")));

  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  // The triggering/first field is of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(0)),
            GetPassportNumber(passport_entity));
}

// Tests that there are no suggestions if the existing entities don't match the
// triggering field.
TEST_F(AutofillAiSuggestionsTest, NonMatchingEntity_DoNoReturnSuggestions) {
  EntityInstance drivers_license_entity =
      test::GetDriversLicenseEntityInstance();
  SetEntities({drivers_license_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER});
  EXPECT_THAT(CreateFillingSuggestions(field(0)), IsEmpty());
}

// Tests that suggestions whose structured attribute would have empty text for
// the value to fill into the triggering field are not shown.
TEST_F(AutofillAiSuggestionsTest, EmptyMainTextForStructuredAttribute) {
  EntityInstance passport = MakePassportWithRandomGuid({.name = u"Miller"});
  SetEntities({passport});

  base::optional_ref<const AttributeInstance> name =
      passport.attribute(AttributeType(AttributeTypeName::kPassportName));
  ASSERT_TRUE(name);
  ASSERT_EQ(name->GetInfo(NAME_FIRST, kAppLocaleUS, std::nullopt), u"");
  ASSERT_EQ(name->GetInfo(NAME_LAST, kAppLocaleUS, std::nullopt), u"Miller");

  SetForm({NAME_FIRST, NAME_LAST, PASSPORT_NUMBER});

  EXPECT_THAT(CreateFillingSuggestions(field(0)), IsEmpty());
  EXPECT_THAT(CreateFillingSuggestions(field(1)), Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestion_DedupeSuggestions) {
  EntityInstance passport1 = MakePassportWithRandomGuid();
  EntityInstance passport2 = MakePassportWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1"});
  EntityInstance passport3 =
      MakePassportWithRandomGuid({.expiry_date = u"2001-12-01"});
  EntityInstance passport4 =
      MakePassportWithRandomGuid({.expiry_date = nullptr});
  SetEntities({passport1, passport2, passport3, passport4});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});

  // `passport3` is deduped because there is no expiry date in the form and its
  // remaining attributes are a subset of `passport1`.
  // `passport4` is deduped because it is a proper subset of `passport1`.
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasMainText(GetPassportName(passport2)),
                             HasMainText(GetPassportName(passport1))));
}

// Tests that an "Undo Autofill" suggestion is appended if the trigger field
// is autofilled.
TEST_F(AutofillAiSuggestionsTest, GetFillingSuggestions_Undo) {
  SetEntities({MakePassportWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              Not(Contains(HasType(SuggestionType::kUndoOrClear))));
  field(0).set_is_autofilled(true);
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              Contains(HasType(SuggestionType::kUndoOrClear)));
}

TEST_F(AutofillAiSuggestionsTest, LabelGeneration_SingleEntity_NoLabelAdded) {
  SetEntities({MakePassportWithRandomGuid()});
  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport")));
}

// Tests that the existence of an entity that does not fill the triggering field
// still affects label generation.
TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_SingleSuggestion_OtherEntitiesFillOtherFieldsInForm_LabelAdded) {
  SetEntities(
      {MakeVehicleWithRandomGuid({.plate = nullptr,
                                  .make = nullptr,
                                  .model = nullptr,
                                  .year = nullptr}),
       MakeVehicleWithRandomGuid({.name = nullptr, .number = nullptr})});
  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_VIN});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Vehicle · BMW · Series 2")));
}

// Test that if focused field (here: passport number) is not the highest-ranking
// disambiguating label (passport name), we the latter as a label.
TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_TwoSuggestions_SameMainText_AddTopDifferentiatingLabel) {
  SetEntities({MakePassportWithRandomGuid(),
               MakePassportWithRandomGuid(
                   {.name = u"Machado de Assis", .number = u"123"})});
  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Machado de Assis")));
}

// Note that because the main text is the top disambiguating field (and is
// different across entities), we do not need to add a label.
TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_DifferentMainText_DoNotAddDifferentiatingLabel) {
  SetEntities({MakePassportWithRandomGuid(),
               MakePassportWithRandomGuid(
                   {.name = u"Machado de Assis", .country = u"Brazil"})});

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport"), HasLabel(u"Passport")));
}

// Note that while the main text is the top disambiguating field, we need
// further labels since it is the same in both suggestions.
TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_SameMainText_AddDifferentiatingLabel) {
  SetEntities({MakePassportWithRandomGuid(),
               MakePassportWithRandomGuid({.country = u"Brazil"})});

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Sweden"),
                             HasLabel(u"Passport · Brazil")));
}

// Note that because the main text is not the top disambiguating field, we do
// need to add a label, even when all main texts are different and the the main
// text disambiguating itself (but not the top one).
TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_MainTextIsNotTopDisambiguatingType_addDifferentiatingLabel) {
  SetEntities({MakePassportWithRandomGuid(),
               MakePassportWithRandomGuid(
                   {.name = u"Machado de Assis", .country = u"Brazil"})});

  // Passport country is a disambiguating text, meaning it can be used to
  // further differentiate passport labels when the top type (passport name) is
  // the same. However, we still add the top differentiating label as a label,
  // as we always prioritize having it.
  SetForm({PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Machado de Assis")));
}

// Note that in this case all entities have the same maker, so it is removed
// from the possible list of labels.
TEST_F(AutofillAiSuggestionsTest,
       LabelGeneration_ThreeSuggestions_AddDifferentiatingLabel) {
  SetEntities({MakeVehicleWithRandomGuid(),
               MakeVehicleWithRandomGuid({.model = u"Series 3"}),
               MakeVehicleWithRandomGuid({.name = u"Diego Maradona"})});
  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_MODEL, NAME_FULL});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Vehicle · Series 2 · Knecht Ruprecht"),
                             HasLabel(u"Vehicle · Series 3 · Knecht Ruprecht"),
                             HasLabel(u"Vehicle · Series 2 · Diego Maradona")));
}

TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_ThreeSuggestions_WithMissingValues_AddDifferentiatingLabel) {
  SetEntities(
      {MakePassportWithRandomGuid({.country = u"Brazil"}),
       // This passport can only fill the triggering name field and has no
       // country data label to add.
       MakePassportWithRandomGuid({.number = u"9876", .country = nullptr}),
       MakePassportWithRandomGuid()});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  EXPECT_THAT(
      CreateFillingSuggestions(field(0)),
      SuggestionsAre(HasLabel(u"Passport · Brazil"), HasLabel(u"Passport"),
                     HasLabel(u"Passport · Sweden")));
}

// Test that the non-disambiguating attributes (here: the expiry dates) do not
// occur in the labels.
TEST_F(
    AutofillAiSuggestionsTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentExpiryDates_DoNotAddDifferentiatingLabel) {
  SetEntities({MakePassportWithRandomGuid(),
               MakePassportWithRandomGuid({.expiry_date = u"2018-12-29"})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY, NAME_FULL,
           PASSPORT_EXPIRATION_DATE});
  EXPECT_THAT(CreateFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport"), HasLabel(u"Passport")));
}

}  // namespace
}  // namespace autofill
