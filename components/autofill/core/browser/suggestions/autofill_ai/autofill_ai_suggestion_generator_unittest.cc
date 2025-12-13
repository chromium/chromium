// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"

#include <memory>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using enum SuggestionType;

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

class AutofillAiSuggestionGeneratorTest : public testing::Test {
 public:
  AutofillAiSuggestionGeneratorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiServerModel,
                              features::kAutofillAiNationalIdCard,
                              features::kAutofillAiKnownTravelerNumber,
                              features::kAutofillAiRedressNumber,
                              features::kAutofillAiWalletFlightReservation},
        /*disabled_features=*/{});
    autofill_client_.set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client_.GetPrefs(), autofill_client_.GetIdentityManager(),
            autofill_client_.GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    autofill_client_.SetUpPrefsAndIdentityForAutofillAi();
    generator_ = std::make_unique<AutofillAiSuggestionGenerator>();
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

  void SetEntities(std::vector<EntityInstance> entities) {
    entities_ = std::move(entities);
    for (EntityInstance& entity : entities_) {
      edm().AddOrUpdateEntityInstance(entity);
    }
    webdata_helper().WaitUntilIdle();
  }

  std::optional<std::u16string> GetFillValueForField(
      const Suggestion::AutofillAiPayload& payload,
      const AutofillField& field) {
    auto it = std::ranges::find(entities_, payload.guid, &EntityInstance::guid);
    if (it == entities_.end()) {
      return std::nullopt;
    }
    const EntityInstance& entity = *it;

    std::vector<AutofillFieldWithAttributeType> fields_and_types =
        RationalizeAndDetermineAttributeTypes(*form_structure_, field.section(),
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
    return attribute->GetInfo(field.Type().GetAutofillAiType(entity.type()),
                              kAppLocaleUS, field.format_string());
  }

  std::vector<Suggestion> CreateAutofillAiFillingSuggestions(
      const AutofillField& field) {
    AutofillAiManager manager(&autofill_client_, nullptr);
    std::vector<Suggestion> suggestions =
        manager.GetSuggestions(form_structure(), field);
    return suggestions;
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillAiSuggestionGenerator& generator() { return *generator_; }
  EntityDataManager& edm() { return *autofill_client_.GetEntityDataManager(); }
  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }
  FormData form() { return form_structure_->ToFormData(); }
  FormFieldData& field_data() { return *form_structure_->fields()[0]; }
  FormStructure& form_structure() { return *form_structure_; }
  AutofillField& field(size_t i = 0) { return *form_structure_->fields()[i]; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AutofillAiSuggestionGenerator> generator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient autofill_client_;
  std::vector<EntityInstance> entities_;
  std::optional<FormStructure> form_structure_;
};

std::u16string GetFlightReservationName(const EntityInstance& entity) {
  return entity
      .attribute(
          AttributeType(AttributeTypeName::kFlightReservationPassengerName))
      ->GetCompleteInfo(kAppLocaleUS);
}

std::u16string GetPassportName(const EntityInstance& entity) {
  return entity.attribute(AttributeType(AttributeTypeName::kPassportName))
      ->GetCompleteInfo(kAppLocaleUS);
}

std::u16string GetPassportNumber(const EntityInstance& entity) {
  return entity.attribute(AttributeType(AttributeTypeName::kPassportNumber))
      ->GetCompleteInfo(kAppLocaleUS);
}

std::u16string GetDriversLicenseName(const EntityInstance& entity) {
  return entity
      .attribute(AttributeType(AttributeTypeName::kDriversLicenseName))
      ->GetCompleteInfo(kAppLocaleUS);
}

TEST_F(AutofillAiSuggestionGeneratorTest, GeneratesAutofillAiSuggestions) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kAutofillAi,
                        testing::SizeIs(1))))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return !saved_on_suggestion_data_returned_argument.second.empty();
      }));

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(
                  FillingProduct::kAutofillAi,
                  ElementsAre(HasType(kFillAutofillAi), HasType(kSeparator),
                              HasType(kManageAutofillAi)))))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return !saved_on_suggestions_generated_argument.second.empty();
      }));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       NoSuggestionDataIfEntityDoesNotProduceValue) {
  SetForm({PASSPORT_NUMBER});
  // Driving licence does not fit into passport number field.
  SetEntities({test::GetDriversLicenseEntityInstanceWithRandomGuid()});

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kAutofillAi,
                        IsEmpty())))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return saved_on_suggestion_data_returned_argument.first ==
               SuggestionGenerator::SuggestionDataSource::kAutofillAi;
      }));
}

TEST_F(AutofillAiSuggestionGeneratorTest, NoSuggestionsIfNoEntities) {
  SetForm({PASSPORT_NUMBER});

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kAutofillAi,
                        IsEmpty())))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return saved_on_suggestion_data_returned_argument.first ==
               SuggestionGenerator::SuggestionDataSource::kAutofillAi;
      }));

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(FillingProduct::kAutofillAi, IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.first ==
               FillingProduct::kAutofillAi;
      }));
}

// Tests that no suggestions are generated when the field has a non-Autofill AI
// type.
TEST_F(AutofillAiSuggestionGeneratorTest, NoSuggestionsOnNonAiField) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid()});
  SetForm({ADDRESS_HOME_ZIP, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
}

TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestion_PassportEntity) {
  EntityInstance passport_entity =
      test::GetPassportEntityInstanceWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

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

// Tests that the flight icon is shown for flight reservation entities.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_FlightReservationEntity_HasFlightIcon) {
  SetEntities({test::GetFlightReservationEntityInstanceWithRandomGuid()});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
           FLIGHT_RESERVATION_CONFIRMATION_CODE});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kFlight));
}

TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestion_PrefixMatching) {
  EntityInstance passport_prefix_matches =
      test::GetPassportEntityInstanceWithRandomGuid({.name = u"Jon Doe"});
  EntityInstance passport_prefix_does_not_match =
      test::GetPassportEntityInstanceWithRandomGuid({.name = u"Harry Potter"});

  SetEntities({passport_prefix_matches, passport_prefix_does_not_match});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  field(0).set_value(u"J");

  // There should be only one suggestion whose main text matches is a prefix of
  // the value already existing in the triggering field.
  // Note that there is one separator and one footer suggestion as well.
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      SuggestionsAre(HasMainText(GetPassportName(passport_prefix_matches))));
}

// Tests that no prefix matching is performed if the attribute that would be
// filled into the triggering field is obfuscated.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestionNoPrefixMatchingForObfuscatedAttributes) {
  SetEntities(
      {test::GetPassportEntityInstanceWithRandomGuid({.number = u"12345"})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  field(0).set_value(u"12");
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_SkipFieldsThatDoNotMatchTheTriggeringFieldSection) {
  EntityInstance passport_entity =
      test::GetPassportEntityInstanceWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE});

  field(0).set_section(Section::FromAutocomplete(Section::Autocomplete("foo")));
  field(1).set_section(Section::FromAutocomplete(Section::Autocomplete("bar")));
  ASSERT_NE(field(0).section(), field(1).section());

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              SuggestionsAre(HasMainText(GetPassportNumber(passport_entity))));
  EXPECT_THAT(suggestions,
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump")));

  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  // The triggering/first field is of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(0)),
            GetPassportNumber(passport_entity));
}

// Tests that there are no suggestions if the existing entities don't match the
// triggering field.
TEST_F(AutofillAiSuggestionGeneratorTest,
       NonMatchingEntity_DoNoReturnSuggestions) {
  EntityInstance drivers_license_entity =
      test::GetDriversLicenseEntityInstance();
  SetEntities({drivers_license_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
}

// Tests that suggestions whose structured attribute would have empty text for
// the value to fill into the triggering field are not shown.
TEST_F(AutofillAiSuggestionGeneratorTest, EmptyMainTextForStructuredAttribute) {
  EntityInstance passport =
      test::GetPassportEntityInstanceWithRandomGuid({.name = u"Miller"});
  SetEntities({passport});

  base::optional_ref<const AttributeInstance> name =
      passport.attribute(AttributeType(AttributeTypeName::kPassportName));
  ASSERT_TRUE(name);
  ASSERT_EQ(name->GetInfo(NAME_FIRST, kAppLocaleUS, std::nullopt), u"");
  ASSERT_EQ(name->GetInfo(NAME_LAST, kAppLocaleUS, std::nullopt), u"Miller");

  SetForm({NAME_FIRST, NAME_LAST, PASSPORT_NUMBER});

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(1)), Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_DedupeSuggestions) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1"});
  EntityInstance passport3 = test::GetPassportEntityInstanceWithRandomGuid(
      {.expiry_date = u"2001-12-01"});
  EntityInstance passport4 =
      test::GetPassportEntityInstanceWithRandomGuid({.expiry_date = nullptr});
  SetEntities({passport1, passport2, passport3, passport4});
  // Sets the usage such that the entities are frequency ranked as `passport2`,
  // `passport1`.
  edm().RecordEntityUsed(passport2.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  SetForm({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});

  // `passport3` is deduped because there is no expiry date in the form and its
  // remaining attributes are a subset of `passport1`.
  // `passport4` is deduped because it is a proper subset of `passport1`.
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasMainText(GetPassportName(passport2)),
                             HasMainText(GetPassportName(passport1))));
}

// Test that if several entities are the same, only the last server entity
// suggestion is shown to the user.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_DedupeSuggestions_FavorServerSuggestions) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid({});
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid({});
  EntityInstance passport3 = test::GetPassportEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance passport4 = test::GetPassportEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  SetEntities({passport1, passport2, passport3, passport4});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});

  // Sets `passport4` to have been used so that it is ranked higher and is
  // picked instead of `passport3`.
  edm().RecordEntityUsed(passport4.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  // Note that two of the resulting suggestions are a line separator and a
  // footer.
  ASSERT_EQ(suggestions.size(), 3u);
  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(payload->guid, passport4.guid());
  EXPECT_THAT(suggestions,
              SuggestionsAre(HasMainText(GetPassportName(passport4))));
}

// Test that if a server entity is a subset of a local one, we do not favor it.
// Instead we delete it.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_DedupeSuggestions_ServerSuggestionIsSubsetOfLocalSuggestion) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.expiry_date = nullptr,
       .record_type = EntityInstance::RecordType::kServerWallet});
  SetEntities({passport1, passport2});

  SetForm({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY,
           PASSPORT_EXPIRATION_DATE});

  // Sets `passport1` to have been used so that it is ranked higher and is
  // picked instead of `passport2`.
  edm().RecordEntityUsed(passport1.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  // `passport2` is deduped because it is a proper subset of `passport1`.
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  // Note that two of the resulting suggestions are a line separator and a
  // footer.
  ASSERT_EQ(suggestions.size(), 3u);
  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(payload->guid, passport1.guid());
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasMainText(GetPassportName(passport1))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_GroupEntitiesOfSameType) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno", .use_count = 1});
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1", .use_count = 10});
  EntityInstance driversLicense1 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid({.use_count = 9});
  EntityInstance driversLicense2 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Pink", .use_count = 8});
  SetEntities({passport1, passport2, driversLicense1, driversLicense2});
  SetForm({NAME_FULL, PASSPORT_NUMBER, DRIVERS_LICENSE_NUMBER});

  // `passport1` comes before vehicle entities because the entity of highest
  // frecency is also a passport entity.
  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res, SuggestionsAre(HasMainText(GetPassportName(passport2)),
                          HasMainText(GetPassportName(passport1)),
                          HasMainText(GetDriversLicenseName(driversLicense1)),
                          HasMainText(GetDriversLicenseName(driversLicense2))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_CustomOrderingForFlightReservation) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno", .use_count = 16});
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1", .use_count = 15});
  EntityInstance flight_reservation1 =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.name = u"Peter",
           .departure_time = base::Time::UnixEpoch(),
           .use_count = 10});
  EntityInstance flight_reservation2 =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.name = u"Jacob",
           .departure_time = base::Time::UnixEpoch() + base::Days(1),
           .use_count = 12});
  SetEntities({passport1, passport2, flight_reservation1, flight_reservation2});
  SetForm({NAME_FULL, PASSPORT_NUMBER, FLIGHT_RESERVATION_FLIGHT_NUMBER});

  // Flight reservation entities come before Passport entities, because they
  // have frecency_override set. `flight_reservation1` comes before
  // `flight_reservation2` since the entities are sorted by departure date.
  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res,
      SuggestionsAre(HasMainText(GetFlightReservationName(flight_reservation1)),
                     HasMainText(GetFlightReservationName(flight_reservation2)),
                     HasMainText(GetPassportName(passport1)),
                     HasMainText(GetPassportName(passport2))));
}

// Tests that an "Undo Autofill" suggestion is appended if the trigger field
// is autofilled.
TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestions_Undo) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              Not(Contains(HasType(SuggestionType::kUndoOrClear))));
  field(0).set_is_autofilled(true);
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              Contains(HasType(SuggestionType::kUndoOrClear)));
}

// Tests that even when labels aren't needed to disambiguate, we still add one
// label so that the final label isn't empty.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_SingleEntity_AtLeastOneLabelAdded) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump")));
}

// Tests that the existence of an entity that does not fill the triggering field
// still affects label generation.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_SingleSuggestion_OtherEntitiesFillOtherFieldsInForm_LabelAdded) {
  SetEntities({test::GetVehicleEntityInstanceWithRandomGuid({.plate = nullptr,
                                                             .make = nullptr,
                                                             .model = nullptr,
                                                             .year = nullptr}),
               test::GetVehicleEntityInstanceWithRandomGuid(
                   {.name = nullptr, .number = nullptr})});
  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_VIN});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Vehicle · BMW · Series 2")));
}

// Test that if focused field (here: passport number) is not the highest-ranking
// disambiguating label (passport name), we the latter as a label.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_TwoSuggestions_SameMainText_AddTopDifferentiatingLabel) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Machado de Assis", .number = u"123"});
  SetEntities({passport1, passport2});
  // Sets the usage such that the entities are frequency ranked as `passport1`,
  // `passport2`.
  edm().RecordEntityUsed(passport1.guid(), base::Time::Now());
  edm().RecordEntityUsed(passport1.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Machado de Assis")));
}

// Tests that if the main text is the top disambiguating field (and is different
// across entities), we do not need to add a label, but we still add at least
// one label.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_DifferentMainText_AtLeastOneLabel) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid({.use_count = 0}),
               test::GetPassportEntityInstanceWithRandomGuid(
                   {.name = u"Machado de Assis",
                    .country = u"Brazil",
                    .use_count = 1})});

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Brazil"),
                             HasLabel(u"Passport · Sweden")));
}

// Note that while the main text is the top disambiguating field, we need
// further labels since it is the same in both suggestions.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_SameMainText_AddDifferentiatingLabel) {
  EntityInstance passport1 =
      test::GetPassportEntityInstanceWithRandomGuid({.use_count = 1});
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.country = u"Brazil", .use_count = 0});
  SetEntities({passport1, passport2});
  webdata_helper().WaitUntilIdle();

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Sweden"),
                             HasLabel(u"Passport · Brazil")));
}

// Note that because the main text is not the top disambiguating field, we do
// need to add a label, even when all main texts are different and the the main
// text disambiguating itself (but not the top one).
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsNotTopDisambiguatingType_addDifferentiatingLabel) {
  EntityInstance passport1 =
      test::GetPassportEntityInstanceWithRandomGuid({.use_count = 1});
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Machado de Assis", .country = u"Brazil", .use_count = 0});
  SetEntities({passport1, passport2});
  webdata_helper().WaitUntilIdle();

  // Passport country is a disambiguating text, meaning it can be used to
  // further differentiate passport labels when the top type (passport name) is
  // the same. However, we still add the top differentiating label as a label,
  // as we always prioritize having it.
  SetForm({PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Machado de Assis")));
}

// Note that in this case all entities have the same maker, so it is
// removed from the possible list of labels.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_ThreeSuggestions_AddDifferentiatingLabel) {
  EntityInstance vehicle1 =
      test::GetVehicleEntityInstanceWithRandomGuid({.use_count = 2});
  EntityInstance vehicle2 = test::GetVehicleEntityInstanceWithRandomGuid(
      {.model = u"Series 3", .use_count = 1});
  EntityInstance vehicle3 = test::GetVehicleEntityInstanceWithRandomGuid(
      {.name = u"Diego Maradona", .use_count = 0});
  SetEntities({vehicle1, vehicle2, vehicle3});
  webdata_helper().WaitUntilIdle();

  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_MODEL, NAME_FULL});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Vehicle · Series 2 · Knecht Ruprecht"),
                             HasLabel(u"Vehicle · Series 3 · Knecht Ruprecht"),
                             HasLabel(u"Vehicle · Series 2 · Diego Maradona")));
}

TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_ThreeSuggestions_WithMissingValues_AddDifferentiatingLabel) {
  EntityInstance passport1 = test::GetPassportEntityInstanceWithRandomGuid(
      {.country = u"Brazil", .use_count = 2});
  // This passport can only fill the triggering number field and has no country
  // data label to add.
  EntityInstance passport2 = test::GetPassportEntityInstanceWithRandomGuid(
      {.number = u"9876", .country = nullptr, .use_count = 1});
  EntityInstance passport3 =
      test::GetPassportEntityInstanceWithRandomGuid({.use_count = 0});
  SetEntities({passport1, passport2, passport3});
  webdata_helper().WaitUntilIdle();

  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Brazil"),
                             HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Sweden")));
}

// Test that if the non-disambiguating attributes (here: the expiry dates) are
// the only one distinguishing the suggestions, a label is still shown, but that
// would be an equal label from a disambiguating type.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentExpiryDates_AtLeastOneLabel) {
  SetEntities({test::GetPassportEntityInstanceWithRandomGuid({.use_count = 0}),
               test::GetPassportEntityInstanceWithRandomGuid(
                   {.expiry_date = u"2018-12-29", .use_count = 1})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY, NAME_FULL,
           PASSPORT_EXPIRATION_DATE});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                             HasLabel(u"Passport · Pippi Långstrump")));
}

// Test that in flight reservation suggestion generation. The main label is a
// combined airport information one. (Departure - Arrival).
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_CombinedAirportLabel) {
  SetEntities({test::GetFlightReservationEntityInstance()});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              SuggestionsAre(HasLabel(u"Flight · MUC–BEY")));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_DepartureDateDisambiguation) {
  base::Time departure_time;
  ASSERT_TRUE(base::Time::FromUTCString("2025-01-01", &departure_time));
  SetEntities({test::GetFlightReservationEntityInstanceWithRandomGuid(
                   {.ticket_number = u"123", .departure_time = departure_time}),
               test::GetFlightReservationEntityInstanceWithRandomGuid(
                   {.ticket_number = u"234",
                    .departure_time = departure_time + base::Days(1)})});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  EXPECT_THAT(suggestions, SuggestionsAre(HasLabel(u"Flight · Jan 1"),
                                          HasLabel(u"Flight · Jan 2")));
}

// Tests that passenger name is used as a disambiguating label in flight
// reservation suggestions.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_PassengerNameDisambiguation) {
  base::Time departure_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-02-16T15:30:15", &departure_time));
  SetEntities(
      {test::GetFlightReservationEntityInstanceWithRandomGuid({
           .confirmation_code = u"ABC",
           .name = u"John Doe",
           // The departure time is set to 1 hour before the other
           // entity's departure time to ensure deterministic sorting,
           // as flight reservations suggestions are sorted by departure time.
           .departure_time = departure_time - base::Hours(1),
       }),
       test::GetFlightReservationEntityInstanceWithRandomGuid({
           .confirmation_code = u"DEF",
           .name = u"Bob Doe",
           .departure_time = departure_time,
       })});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  EXPECT_THAT(suggestions, SuggestionsAre(HasLabel(u"Flight · John Doe"),
                                          HasLabel(u"Flight · Bob Doe")));
}

// Tests that flight number is used as a disambiguating label in flight
// reservation suggestions.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_FlightNumberDisambiguation) {
  base::Time departure_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-02-16T15:30:15", &departure_time));
  SetEntities(
      {test::GetFlightReservationEntityInstanceWithRandomGuid({
           .flight_number = u"123",
           .ticket_number = u"ABC",
           // The departure time is set to 1 hour before the other
           // entity's departure time to ensure deterministic sorting,
           // as flight reservations suggestions are sorted by departure time.
           .departure_time = departure_time - base::Hours(1),
       }),
       test::GetFlightReservationEntityInstanceWithRandomGuid({
           .flight_number = u"234",
           .ticket_number = u"DEF",
           .departure_time = departure_time,
       })});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  EXPECT_THAT(suggestions, SuggestionsAre(HasLabel(u"Flight · 123"),
                                          HasLabel(u"Flight · 234")));
}

// Tests that the Wallet suggestions show the IPH.
TEST_F(AutofillAiSuggestionGeneratorTest, WalletSuggestionsShowIPH) {
  SetEntities({test::GetVehicleEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kServerWallet})});
  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_VIN});
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  raw_ptr<const base::Feature> kIphFeature =
      &feature_engagement::kIPHAutofillAiValuablesFeature;
  EXPECT_THAT(suggestions, SuggestionsAre(HasIphFeature(kIphFeature)));
}

}  // namespace
}  // namespace autofill
