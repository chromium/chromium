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
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/network/autofill_ai/mock_autofill_ai_personal_context_access_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {
using enum SuggestionType;

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;
using test::GetFlightReservationEntityInstanceWithRandomGuid;
using test::GetOrderEntityInstance;
using test::GetPassportEntityInstance;
using test::GetPassportEntityInstanceWithRandomGuid;
using test::MaskEntityInstance;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::Return;

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

Matcher<const Suggestion&> HasRequiresServerFetch(bool requires_server_fetch) {
  return ResultOf(
      "Suggestion::payload",
      [](const Suggestion& s) {
        return std::get<Suggestion::AutofillAiPayload>(s.payload)
            .requires_server_fetch;
      },
      requires_server_fetch);
}

Matcher<Suggestion> HasAcceptability(Suggestion::Acceptability acceptability) {
  return Field("Suggestion::acceptability", &Suggestion::acceptability,
               acceptability);
}

Matcher<Suggestion> SuggestionTypeHasTextAndAcceptability(
    SuggestionType type,
    const std::u16string& main_text,
    Suggestion::Acceptability acceptability) {
  return AllOf(EqualsSuggestion(type, main_text),
               HasAcceptability(acceptability));
}

auto ChildrenAre(auto&&... matchers) {
  return Field("Suggestion::children", &Suggestion::children,
               ElementsAre(std::forward<decltype(matchers)>(matchers)...));
}

auto IdentityDocSuggestionsAre(auto&&... matchers) {
  return ElementsAre(
      std::forward<decltype(matchers)>(matchers)...,
      EqualsSuggestion(SuggestionType::kSeparator),
      EqualsSuggestion(SuggestionType::kManageAutofillAiIdentityDocs));
}

auto TravelSuggestionsAre(auto&&... matchers) {
  return ElementsAre(std::forward<decltype(matchers)>(matchers)...,
                     EqualsSuggestion(SuggestionType::kSeparator),
                     EqualsSuggestion(SuggestionType::kManageAutofillAiTravel));
}

auto ShoppingSuggestionsAre(auto&&... matchers) {
  return ElementsAre(
      std::forward<decltype(matchers)>(matchers)...,
      EqualsSuggestion(SuggestionType::kSeparator),
      EqualsSuggestion(SuggestionType::kManageAutofillAiShopping));
}

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

class AutofillAiSuggestionGeneratorTest : public testing::Test {
 public:
  explicit AutofillAiSuggestionGeneratorTest(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    autofill_client_.set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client_.GetPrefs(), autofill_client_.GetIdentityManager(),
            autofill_client_.GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr, &pcontext_manager_,
            /*strike_database=*/nullptr,
            /*variation_country_code=*/GeoIpCountryCode("US")));
    autofill_client_.SetUpPrefsAndIdentityForAutofillAi();
    generator_ = std::make_unique<AutofillAiSuggestionGenerator>();
  }

  AutofillAiSuggestionGeneratorTest()
      : AutofillAiSuggestionGeneratorTest(GetDefaultEnabledFeatures(),
                                          /*disabled_features=*/{}) {}

  // Sets the form to one whose `i`th field has type `field_types[i]`.
  void SetForm(const std::vector<FieldType>& field_types) {
    test::FormDescription form_description;
    for (FieldType type : field_types) {
      form_description.fields.emplace_back(
          test::FieldDescription({.role = type}));
    }
    form_structure_.emplace(test::GetFormData(form_description));
    CHECK_EQ(field_types.size(), form_structure_->field_count());
    for (size_t i = 0; i < form_structure_->field_count(); ++i) {
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
      switch (entity.record_type()) {
        case EntityInstance::RecordType::kLocal:
        case EntityInstance::RecordType::kServerWallet:
          edm().AddOrUpdateEntityInstance(entity);
          break;
        case EntityInstance::RecordType::kPersonalContext:
          edm().OnPrefetchContextComplete(pcontext_manager_,
                                          std::vector<EntityInstance>{entity});
          break;
      }
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

 protected:
  static std::vector<base::test::FeatureRef> GetDefaultEnabledFeatures() {
    return {features::kAutofillAiWithDataSchema,
            features::kAutofillAiServerModel,
            features::kAutofillAiWalletPrivatePasses,
            features::kAutofillAiWalletFlightReservation,
            features::kAutofillAiOrder};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AutofillAiSuggestionGenerator> generator_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager>
      pcontext_manager_;
  TestAutofillClient autofill_client_;
  std::vector<EntityInstance> entities_;
  std::optional<FormStructure> form_structure_;
};

// Tests that the suggestions's main text is obfuscated when the triggering
// field is from an attribute type that should be obfuscated.
TEST_F(AutofillAiSuggestionGeneratorTest, SuggestionMainTextIsObfuscated) {
  EntityInstance passport_entity = GetPassportEntityInstanceWithRandomGuid(
      {.number = u"123456", .country = u"Brazil"});
  SetEntities({passport_entity});
  SetForm({PASSPORT_NUMBER});

  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasMainText(GetObfuscatedValue(
          GetPassportNumber(passport_entity), /*visible_suffix_length=*/4))));
}

TEST_F(AutofillAiSuggestionGeneratorTest, GeneratesAutofillAiSuggestions) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(
          SuggestionGenerator::SuggestionDataSource::kAutofillAi,
          ElementsAre(EqualsSuggestion(kFillAutofillAi),
                      EqualsSuggestion(kSeparator),
                      EqualsSuggestion(kManageAutofillAiIdentityDocs)))))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(), client(),
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

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kAutofillAi,
                        IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.first ==
               SuggestionGenerator::SuggestionDataSource::kAutofillAi;
      }));
}

TEST_F(AutofillAiSuggestionGeneratorTest, NoSuggestionsIfNoEntities) {
  SetForm({PASSPORT_NUMBER});

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kAutofillAi,
                        IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.first ==
               SuggestionGenerator::SuggestionDataSource::kAutofillAi;
      }));
}

// Tests that no suggestions are generated when the field has a non-Autofill AI
// type.
TEST_F(AutofillAiSuggestionGeneratorTest, NoSuggestionsOnNonAiField) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({ADDRESS_HOME_ZIP, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
}

TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestion_PassportEntity) {
  EntityInstance passport_entity = GetPassportEntityInstanceWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // There should be only one suggestion whose main text matches the entity
  // value for the passport name.
  EXPECT_THAT(suggestions, IdentityDocSuggestionsAre(
                               HasMainText(GetPassportName(passport_entity))));

  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kPassport));

  // The triggering/first field is of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(0)),
            GetPassportName(passport_entity));
  // The second field in the form is also of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(1)),
            GetPassportNumber(passport_entity));
  // The third field is not of Autofill AI type.
  EXPECT_EQ(GetFillValueForField(*payload, field(2)), std::nullopt);
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_LocalPassportEntity_DoesNotRequireServerFetch) {
  EntityInstance passport_entity = GetPassportEntityInstanceWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});

  // Local passport should not require a server fetch.
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(
                  AllOf(HasMainText(GetPassportName(passport_entity)),
                        HasIcon(Suggestion::Icon::kPassport),
                        HasRequiresServerFetch(false))));
}

// Tests that a masked server entity requires a server fetch when the feature
// is enabled.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_MaskedServerPassport_RequiresServerFetch) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiWalletPrivatePasses);

  EntityInstance passport_entity = MaskEntityInstance(GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet}));
  SetEntities({passport_entity});
  SetForm({PASSPORT_NUMBER});

  // Masked server passport should require a server fetch.
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(AllOf(HasIcon(Suggestion::Icon::kPassport),
                                      HasRequiresServerFetch(true))));
}

TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_MaskedServerPassport_DoesNotRequireServerFetchIfNoSensitiveFieldInForm) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiWalletPrivatePasses);

  EntityInstance passport_entity = MaskEntityInstance(GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet}));
  SetEntities({passport_entity});
  // Form has Name and Issue date, not Passport Number.
  SetForm({NAME_FULL, PASSPORT_ISSUE_DATE});

  // Since the form doesn't ask for any sensitive attribute (like Passport
  // Number), we don't need a server fetch, even if the entity is a masked
  // server entity.
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(
                  AllOf(HasMainText(GetPassportName(passport_entity)),
                        HasRequiresServerFetch(false))));
}

// Tests that the flight icon is shown for flight reservation entities.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_FlightReservationEntity_HasFlightIcon) {
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid()});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
           FLIGHT_RESERVATION_CONFIRMATION_CODE});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kFlight));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Tests that no icon is set when `kAutofillAiNoFillingIconsExperiment` is
// enabled.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_FlightReservationEntity_NoIconIfFeatureIsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiNoFillingIconsExperiment);
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid()});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
           FLIGHT_RESERVATION_CONFIRMATION_CODE});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kNoIcon));
}
#endif

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_PersonalContextEntity_UseSparkIcon) {
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kPersonalContext})});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
           FLIGHT_RESERVATION_CONFIRMATION_CODE});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kFlightSpark));
}

TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextPassportEntity_UsePassportSparkIcon) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kPersonalContext})});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions[0], HasIcon(Suggestion::Icon::kPassportSpark));
}

TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextAndLocalEntities_SuggestedByGeminiLabel) {
  EntityInstance passport_local = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"123456",
       .record_type = EntityInstance::RecordType::kLocal,
       .use_count = 1});
  EntityInstance passport_personal_context =
      GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Harry Potter",
           .number = u"987654",
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 0});
  SetEntities({passport_local, passport_personal_context});
  SetForm({PASSPORT_NUMBER, NAME_FULL});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  ASSERT_GE(suggestions.size(), 2u);

  const Suggestion::AutofillAiPayload* local_payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(local_payload);
  EXPECT_EQ(local_payload->guid, passport_local.guid());
  ASSERT_EQ(suggestions[0].labels.size(), 1u);

  const Suggestion::AutofillAiPayload* pc_payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[1].payload);
  ASSERT_TRUE(pc_payload);
  EXPECT_EQ(pc_payload->guid, passport_personal_context.guid());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ASSERT_EQ(suggestions[1].labels.size(), 2u);
  ASSERT_EQ(suggestions[1].labels[1].size(), 1u);
  EXPECT_EQ(suggestions[1].labels[1][0].value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI));
#else
  ASSERT_EQ(suggestions[1].labels.size(), 1u);
#endif
}

TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestion_PrefixMatching) {
  EntityInstance passport_prefix_matches =
      GetPassportEntityInstanceWithRandomGuid({.name = u"Jon Doe"});
  EntityInstance passport_prefix_does_not_match =
      GetPassportEntityInstanceWithRandomGuid({.name = u"Harry Potter"});

  SetEntities({passport_prefix_matches, passport_prefix_does_not_match});
  SetForm({NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  field(0).set_value(u"J");

  // There should be only one suggestion whose main text matches is a prefix of
  // the value already existing in the triggering field.
  // Note that there is one separator and one footer suggestion as well.
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(
                  HasMainText(GetPassportName(passport_prefix_matches))));
}

// Tests that no prefix matching is performed if the attribute that would be
// filled into the triggering field is obfuscated.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestionNoPrefixMatchingForObfuscatedAttributes) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid({.number = u"12345"})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  field(0).set_value(u"12");
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), Not(IsEmpty()));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_SkipFieldsThatDoNotMatchTheTriggeringFieldSection) {
  EntityInstance passport_entity = GetPassportEntityInstanceWithRandomGuid();
  SetEntities({passport_entity});
  SetForm({PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE});

  field(0).set_section(Section::FromAutocomplete(Section::Autocomplete("foo")));
  field(1).set_section(Section::FromAutocomplete(Section::Autocomplete("bar")));
  ASSERT_NE(field(0).section(), field(1).section());

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      suggestions,
      IdentityDocSuggestionsAre(HasMainText(GetObfuscatedValue(
          GetPassportNumber(passport_entity), /*visible_suffix_length=*/4))));
  EXPECT_THAT(suggestions, IdentityDocSuggestionsAre(
                               HasLabel(u"Passport · Pippi Långstrump")));

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
      GetPassportEntityInstanceWithRandomGuid({.name = u"Miller"});
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
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1"});
  EntityInstance passport3 =
      GetPassportEntityInstanceWithRandomGuid({.expiry_date = u"2001-12-01"});
  EntityInstance passport4 =
      GetPassportEntityInstanceWithRandomGuid({.expiry_date = nullptr});
  SetEntities({passport1, passport2, passport3, passport4});
  // Sets the usage such that the entities are frequency ranked as `passport2`,
  // `passport1`.
  edm().RecordEntityUsed(passport2.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  SetForm({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});

  // `passport3` is deduped because there is no expiry date in the form and its
  // remaining attributes are a subset of `passport1`.
  // `passport4` is deduped because it is a proper subset of `passport1`.
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasMainText(GetPassportName(passport2)),
                                HasMainText(GetPassportName(passport1))));
}

// Test that if several entities are the same, only the last server entity
// suggestion is shown to the user.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_DedupeSuggestions_FavorServerSuggestions) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid({});
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid({});
  EntityInstance passport3 =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  EntityInstance passport4 =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
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
  EXPECT_THAT(suggestions, IdentityDocSuggestionsAre(
                               HasMainText(GetPassportName(passport4))));
}

// Test that if a Local entity and a PersonalContext entity have the
// same data, the Local entity is preferred.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_DedupeSuggestions_FavorLocalOverPersonalContext) {
  EntityInstance passport_local = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"12345",
       .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_pc =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"12345",
           .record_type = EntityInstance::RecordType::kPersonalContext}));
  SetEntities({passport_local, passport_pc});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  // Sets `passport_pc` to have been used so that it is ranked higher by
  // frecency.
  edm().RecordEntityUsed(passport_pc.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // There should be only one suggestion (excluding separator and footer),
  // and it should be the Local one.
  ASSERT_EQ(suggestions.size(), 3u);
  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(payload->guid, passport_local.guid());
  EXPECT_THAT(suggestions, IdentityDocSuggestionsAre(
                               HasMainText(GetPassportName(passport_local))));
}

// Test that if a ServerWallet, Local, and PersonalContext entity have
// the same data, they are prioritized correctly:
// ServerWallet > Local > PersonalContext.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_DedupeSuggestions_FavorServerOverLocalAndPersonalContext) {
  EntityInstance passport_server =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"12345",
           .record_type = EntityInstance::RecordType::kServerWallet}));
  EntityInstance passport_local = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"12345",
       .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_pc =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"12345",
           .record_type = EntityInstance::RecordType::kPersonalContext}));

  SetEntities({passport_server, passport_local, passport_pc});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  // Set frecency: PersonalContext > Local > ServerWallet
  base::Time now = base::Time::Now();
  edm().RecordEntityUsed(passport_pc.guid(), now);
  edm().RecordEntityUsed(passport_local.guid(), now - base::Minutes(1));
  edm().RecordEntityUsed(passport_server.guid(), now - base::Minutes(2));
  webdata_helper().WaitUntilIdle();

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // There should be only one suggestion (excluding separator and footer),
  // and it should be the ServerWallet one because it has the highest priority.
  ASSERT_EQ(suggestions.size(), 3u);
  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(payload->guid, passport_server.guid());
  EXPECT_THAT(suggestions, IdentityDocSuggestionsAre(
                               HasMainText(GetPassportName(passport_server))));
}

// Test that if a server entity is a subset of a local one, we do not favor it.
// Instead we delete it.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_DedupeSuggestions_ServerSuggestionIsSubsetOfLocalSuggestion) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.expiry_date = nullptr,
           .record_type = EntityInstance::RecordType::kServerWallet}));
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
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasMainText(GetPassportName(passport1))));
}

// Test that if a local entity's obfuscated attribute ends in the same suffix as
// a server's obfuscated attribute, we dedupe the local entity.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_DedupeSuggestions_SameSuffix) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Pippi", .number = u"1234567890"});
  EntityInstance passport2 =
      MaskEntityInstance(GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Pippi",
           .number = u"7890",
           .record_type = EntityInstance::RecordType::kServerWallet}));
  SetEntities({passport1, passport2});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  ASSERT_EQ(suggestions.size(), 3u);
  const Suggestion::AutofillAiPayload* payload =
      std::get_if<Suggestion::AutofillAiPayload>(&suggestions[0].payload);
  ASSERT_TRUE(payload);
  EXPECT_EQ(payload->guid, passport2.guid());
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_GroupEntitiesOfSameType) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno", .use_count = 1});
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1", .use_count = 10});
  EntityInstance drivers_license1 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid({.use_count = 9});
  EntityInstance drivers_license2 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Pink", .use_count = 8});
  SetEntities({passport1, passport2, drivers_license1, drivers_license2});
  SetForm({NAME_FULL, PASSPORT_NUMBER, DRIVERS_LICENSE_NUMBER});

  // `passport1` comes before vehicle entities because the entity of highest
  // frecency is also a passport entity.
  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, IdentityDocSuggestionsAre(
                       HasMainText(GetPassportName(passport2)),
                       HasMainText(GetPassportName(passport1)),
                       HasMainText(GetDriversLicenseName(drivers_license1)),
                       HasMainText(GetDriversLicenseName(drivers_license2))));
}

// Test that within the same entity type, PersonalContext entities are ordered
// after non-PersonalContext entities, even if they have higher frecency.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_OrderPersonalContextAfterOtherEntityTypes_SameType) {
  EntityInstance passport_local_1 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno",
       .number = u"11111",
       .use_date = test::kJune2017 - base::Days(1),
       .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_local_2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Alice",
       .number = u"22222",
       .use_date = test::kJune2017 - base::Days(2),
       .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_pc = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"33333",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({passport_local_1, passport_local_2, passport_pc});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));

  EXPECT_THAT(res, IdentityDocSuggestionsAre(
                       HasMainText(GetPassportName(passport_local_1)),
                       HasMainText(GetPassportName(passport_local_2)),
                       HasMainText(GetPassportName(passport_pc))));
}

// Test that across different entity types, PersonalContext entities are ordered
// after non-PersonalContext entities of other types, even if they have higher
// frecency.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_OrderPersonalContextAfterOtherEntityTypes_DifferentTypes) {
  EntityInstance passport_pc = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"33333",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance drivers_license_local =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Pink",
           .number = u"44444",
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kLocal});

  SetEntities({passport_pc, drivers_license_local});
  SetForm({NAME_FULL, PASSPORT_NUMBER, DRIVERS_LICENSE_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));

  EXPECT_THAT(res,
              IdentityDocSuggestionsAre(
                  HasMainText(GetDriversLicenseName(drivers_license_local)),
                  HasMainText(GetPassportName(passport_pc))));
}

// Test that entities are first partitioned by RecordType (non-PersonalContext
// before PersonalContext), and then within each partition, grouped by
// EntityType and sorted by frecency.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_OrderPersonalContextAfterOtherEntityTypes_RecordTypeBeforeEntityType) {
  EntityInstance passport_local = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno",
       .number = u"11111",
       .use_date = test::kJune2017 - base::Days(1),
       .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_pc = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"22222",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance drivers_license_local =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Pink",
           .number = u"33333",
           .use_date = test::kJune2017 - base::Days(2),
           .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance drivers_license_pc =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"White",
           .number = u"44444",
           .use_date = test::kJune2017 - base::Days(2),
           .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities(
      {passport_local, passport_pc, drivers_license_local, drivers_license_pc});
  SetForm({NAME_FULL, PASSPORT_NUMBER, DRIVERS_LICENSE_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));

  EXPECT_THAT(res,
              IdentityDocSuggestionsAre(
                  HasMainText(GetPassportName(passport_local)),
                  HasMainText(GetDriversLicenseName(drivers_license_local)),
                  HasMainText(GetPassportName(passport_pc)),
                  HasMainText(GetDriversLicenseName(drivers_license_pc))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_CustomOrderingForFlightReservation) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno", .use_count = 16});
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe", .number = u"927908CYGAS1", .use_count = 15});
  EntityInstance flight_reservation1 =
      GetFlightReservationEntityInstanceWithRandomGuid(
          {.name = u"Peter",
           .departure_time = base::Time::UnixEpoch(),
           .use_date = test::kJune2017,
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 15});
  EntityInstance flight_reservation2 =
      GetFlightReservationEntityInstanceWithRandomGuid(
          {.name = u"Jacob",
           .departure_time = base::Time::UnixEpoch() + base::Days(1),
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 10});
  SetEntities({passport1, passport2, flight_reservation1, flight_reservation2});
  SetForm({NAME_FULL, PASSPORT_NUMBER, FLIGHT_RESERVATION_FLIGHT_NUMBER});

  // Passport entities come before Flight reservation, because flights
  // come from personal context. `flight_reservation2` comes before
  // `flight_reservation1` since the entities are sorted descending by departure
  // date.
  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res,
      ElementsAre(HasMainText(GetPassportName(passport1)),
                  HasMainText(GetPassportName(passport2)),
                  HasMainText(GetFlightReservationName(flight_reservation2)),
                  HasMainText(GetFlightReservationName(flight_reservation1)),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAutofillAi)));
}

// Test that PersonalContext Passport entities are sorted descending by
// expiration date, even if the one expiring later has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_PersonalContextOrdering_PassportExpirationDate) {
  EntityInstance passport_sooner = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno",
       .number = u"11111",
       .expiry_date = u"2026-08-01",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext,
       .use_count = 15});
  EntityInstance passport_later = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Jon Doe",
       .number = u"22222",
       .expiry_date = u"2029-08-01",
       .use_date = test::kJune2017 - base::Days(10),
       .record_type = EntityInstance::RecordType::kPersonalContext,
       .use_count = 10});

  SetEntities({passport_later, passport_sooner});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, IdentityDocSuggestionsAre(
                       HasMainText(GetPassportName(passport_later)),
                       HasMainText(GetPassportName(passport_sooner))));
}

// Test that PersonalContext DriversLicense entities are sorted descending by
// expiration date.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextOrdering_DriversLicenseExpirationDate) {
  EntityInstance drivers_license_sooner =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Sooner",
           .expiration_date = u"2026-08-01",
           .use_date = test::kJune2017,
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 15});
  EntityInstance drivers_license_later =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.name = u"Mr Later",
           .expiration_date = u"2029-08-01",
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 10});

  SetEntities({drivers_license_later, drivers_license_sooner});
  SetForm({NAME_FULL, DRIVERS_LICENSE_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res,
              IdentityDocSuggestionsAre(
                  HasMainText(GetDriversLicenseName(drivers_license_later)),
                  HasMainText(GetDriversLicenseName(drivers_license_sooner))));
}

// Test that PersonalContext Vehicle entities are sorted by plate number
// alphanumerically (case-insensitive), even if the one with a later plate
// number has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_PersonalContextOrdering_VehiclePlateNumber) {
  EntityInstance vehicle_a = test::GetVehicleEntityInstanceWithRandomGuid(
      {.plate = u"abc-123",
       .number = u"11111",
       .use_date = test::kJune2017 - base::Days(10),
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance vehicle_z = test::GetVehicleEntityInstanceWithRandomGuid(
      {.plate = u"XYZ-999",
       .number = u"22222",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({vehicle_z, vehicle_a});
  SetForm({VEHICLE_LICENSE_PLATE, VEHICLE_VIN});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, TravelSuggestionsAre(HasMainText(u"abc-123"),
                                        HasMainText(u"XYZ-999")));
}

// Test that when sorting PersonalContext entities of the same type, an entity
// with the sorting attribute present comes before one without it, even
// if the entity without the attribute has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestion_PersonalContextOrdering_PresentVsMissingAttribute) {
  EntityInstance passport_with_expiry = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno",
       .number = u"11111",
       .expiry_date = u"2026-08-01",
       .use_date = test::kJune2017 - base::Days(10),
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_without_expiry =
      GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"22222",
           .expiry_date = nullptr,
           .use_date = test::kJune2017,
           .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({passport_without_expiry, passport_with_expiry});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, IdentityDocSuggestionsAre(
                       HasMainText(GetPassportName(passport_with_expiry)),
                       HasMainText(GetPassportName(passport_without_expiry))));
}

// Test that when two PersonalContext entities of the same type have tied
// sorting attribute values (e.g. same expiration date), the comparison falls
// back to FrecencyOrder.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextOrdering_TiedAttribute_FallsBackToFrecency) {
  EntityInstance passport_frecent = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Bruno",
       .number = u"11111",
       .expiry_date = u"2027-08-01",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_less_frecent =
      GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"22222",
           .expiry_date = u"2027-08-01",
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({passport_less_frecent, passport_frecent});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, IdentityDocSuggestionsAre(
                       HasMainText(GetPassportName(passport_frecent)),
                       HasMainText(GetPassportName(passport_less_frecent))));
}

// Test that when two PersonalContext entities of the same type both lack the
// sorting attribute (e.g. both have missing expiration dates), the comparison
// falls back to FrecencyOrder.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextOrdering_BothMissingAttribute_FallsBackToFrecency) {
  EntityInstance passport_frecent_no_expiry =
      GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Bruno",
           .number = u"11111",
           .expiry_date = nullptr,
           .use_date = test::kJune2017,
           .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_less_frecent_no_expiry =
      GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"22222",
           .expiry_date = nullptr,
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({passport_less_frecent_no_expiry, passport_frecent_no_expiry});
  SetForm({NAME_FULL, PASSPORT_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res, IdentityDocSuggestionsAre(
               HasMainText(GetPassportName(passport_frecent_no_expiry)),
               HasMainText(GetPassportName(passport_less_frecent_no_expiry))));
}

// Test that when sorting PersonalContext entities of entity types without
// sorting rules (e.g. KnownTravelerNumber), the comparison falls back to
// FrecencyOrder.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    GetFillingSuggestion_PersonalContextOrdering_OtherEntityType_FallsBackToFrecency) {
  EntityInstance ktn_frecent =
      test::GetKnownTravelerNumberInstanceWithRandomGuid(
          {.name = u"Bruno",
           .number = u"11111",
           .use_date = test::kJune2017,
           .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance ktn_less_frecent =
      test::GetKnownTravelerNumberInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"22222",
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({ktn_less_frecent, ktn_frecent});
  SetForm({KNOWN_TRAVELER_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res, TravelSuggestionsAre(HasMainText(GetObfuscatedValue(
                                    u"11111", /*visible_suffix_length=*/4)),
                                HasMainText(GetObfuscatedValue(
                                    u"22222", /*visible_suffix_length=*/4))));
}

// Tests that a kPersonalContextNotice suggestion is appended if the trigger
// field contains a personal context entity and the notice should be shown.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestions_PersonalContextNotice) {
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kPersonalContext})});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER});

  client().set_should_show_personal_context_ambient_autofill_notice(true);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      Not(Contains(EqualsSuggestion(SuggestionType::kPersonalContextNotice))));
#else
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      Contains(EqualsSuggestion(SuggestionType::kPersonalContextNotice)));
#endif
}

// Tests that a kPersonalContextNotice suggestion is not appended if the
// trigger field does not contain a personal context entity, even if the notice
// should be shown.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestions_PersonalContextNotice_NoPersonalContextEntity) {
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kLocal})});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER});

  client().set_should_show_personal_context_ambient_autofill_notice(true);

  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      Not(Contains(EqualsSuggestion(SuggestionType::kPersonalContextNotice))));
}

// Tests that a kPersonalContextNotice suggestion is not appended if the
// notice should not be shown, even if the trigger field contains a personal
// context entity.
TEST_F(AutofillAiSuggestionGeneratorTest,
       GetFillingSuggestions_PersonalContextNotice_ShouldNotShowNotice) {
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kPersonalContext})});
  SetForm({FLIGHT_RESERVATION_FLIGHT_NUMBER});

  client().set_should_show_personal_context_ambient_autofill_notice(false);

  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      Not(Contains(EqualsSuggestion(SuggestionType::kPersonalContextNotice))));
}

// Tests that an "Undo Autofill" suggestion is appended if the trigger field
// is autofilled.
TEST_F(AutofillAiSuggestionGeneratorTest, GetFillingSuggestions_Undo) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              Not(Contains(EqualsSuggestion(SuggestionType::kUndoOrClear))));
  field_data().set_is_autofilled_according_to_renderer(true);
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              Contains(EqualsSuggestion(SuggestionType::kUndoOrClear)));
}

// Tests that even when labels aren't needed to disambiguate, we still add one
// label so that the final label isn't empty.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_SingleEntity_AtLeastOneLabelAdded) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasLabel(u"Passport · Pippi Långstrump")));
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
              TravelSuggestionsAre(HasLabel(u"Vehicle · BMW · Series 2")));
}

// Test that if focused field (here: passport number) is not the highest-ranking
// disambiguating label (passport name), we the latter as a label.
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_TwoSuggestions_SameMainText_AddTopDifferentiatingLabel) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid();
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Machado de Assis", .number = u"123"});
  SetEntities({passport1, passport2});
  // Sets the usage such that the entities are frequency ranked as `passport1`,
  // `passport2`.
  edm().RecordEntityUsed(passport1.guid(), base::Time::Now());
  edm().RecordEntityUsed(passport1.guid(), base::Time::Now());
  webdata_helper().WaitUntilIdle();

  SetForm({PASSPORT_NUMBER, NAME_FULL});
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                                HasLabel(u"Passport · Machado de Assis")));
}

// Tests that if the main text is the top disambiguating field (and is different
// across entities), we do not need to add a label, but we still add at least
// one label.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_DifferentMainText_AtLeastOneLabel) {
  SetEntities(
      {GetPassportEntityInstanceWithRandomGuid({.use_count = 0}),
       GetPassportEntityInstanceWithRandomGuid({.name = u"Machado de Assis",
                                                .country = u"Brazil",
                                                .use_count = 1})});

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(HasLabel(u"Passport · Brazil"),
                                        HasLabel(u"Passport · Sweden")));
}

// Note that while the main text is the top disambiguating field, we need
// further labels since it is the same in both suggestions.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsDisambiguating_SameMainText_AddDifferentiatingLabel) {
  EntityInstance passport1 =
      GetPassportEntityInstanceWithRandomGuid({.use_count = 1});
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.country = u"Brazil", .use_count = 0});
  SetEntities({passport1, passport2});
  webdata_helper().WaitUntilIdle();

  // Note that passport name is the first at the rank of disambiguating texts.
  SetForm({NAME_FULL, PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(HasLabel(u"Passport · Sweden"),
                                        HasLabel(u"Passport · Brazil")));
}

// Note that because the main text is not the top disambiguating field, we do
// need to add a label, even when all main texts are different and the the main
// text disambiguating itself (but not the top one).
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_MainTextIsNotTopDisambiguatingType_addDifferentiatingLabel) {
  EntityInstance passport1 =
      GetPassportEntityInstanceWithRandomGuid({.use_count = 1});
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.name = u"Machado de Assis", .country = u"Brazil", .use_count = 0});
  SetEntities({passport1, passport2});
  webdata_helper().WaitUntilIdle();

  // Passport country is a disambiguating text, meaning it can be used to
  // further differentiate passport labels when the top type (passport name) is
  // the same. However, we still add the top differentiating label as a label,
  // as we always prioritize having it.
  SetForm({PASSPORT_ISSUING_COUNTRY, PASSPORT_NUMBER});
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
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
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      TravelSuggestionsAre(HasLabel(u"Vehicle · Series 2 · Knecht Ruprecht"),
                           HasLabel(u"Vehicle · Series 3 · Knecht Ruprecht"),
                           HasLabel(u"Vehicle · Series 2 · Diego Maradona")));
}

TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_ThreeSuggestions_WithMissingValues_AddDifferentiatingLabel) {
  EntityInstance passport1 = GetPassportEntityInstanceWithRandomGuid(
      {.country = u"Brazil", .use_count = 2});
  // This passport can only fill the triggering number field and has no country
  // data label to add.
  EntityInstance passport2 = GetPassportEntityInstanceWithRandomGuid(
      {.number = u"9876", .country = nullptr, .use_count = 1});
  EntityInstance passport3 =
      GetPassportEntityInstanceWithRandomGuid({.use_count = 0});
  SetEntities({passport1, passport2, passport3});
  webdata_helper().WaitUntilIdle();

  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY});
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasLabel(u"Passport · Brazil"),
                                HasLabel(u"Passport · Pippi Långstrump"),
                                HasLabel(u"Passport · Sweden")));
}

// Test that if the non-disambiguating attributes (here: the expiry dates) are
// the only one distinguishing the suggestions, a label is still shown, but that
// would be an equal label from a disambiguating type.
TEST_F(
    AutofillAiSuggestionGeneratorTest,
    LabelGeneration_TwoSuggestions_PassportsWithDifferentExpiryDates_AtLeastOneLabel) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid({.use_count = 0}),
               GetPassportEntityInstanceWithRandomGuid(
                   {.expiry_date = u"2018-12-29", .use_count = 1})});
  SetForm({PASSPORT_NUMBER, PASSPORT_ISSUING_COUNTRY, NAME_FULL,
           PASSPORT_EXPIRATION_DATE});
  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      IdentityDocSuggestionsAre(HasLabel(u"Passport · Pippi Långstrump"),
                                HasLabel(u"Passport · Pippi Långstrump")));
}

// Test that in flight reservation suggestion generation. The main label is a
// combined airport information one. (Departure - Arrival).
TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_CombinedAirportLabel) {
  SetEntities({test::GetFlightReservationEntityInstance()});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});
  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              TravelSuggestionsAre(HasLabel(u"Flight · MUC–BEY")));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       LabelGeneration_FlightReservation_DepartureDateDisambiguation) {
  base::Time departure_time;
  ASSERT_TRUE(base::Time::FromUTCString("2025-01-01", &departure_time));
  SetEntities({GetFlightReservationEntityInstanceWithRandomGuid(
                   {.ticket_number = u"123", .departure_time = departure_time}),
               GetFlightReservationEntityInstanceWithRandomGuid(
                   {.ticket_number = u"234",
                    .departure_time = departure_time + base::Days(1)})});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  EXPECT_THAT(suggestions, TravelSuggestionsAre(HasLabel(u"Flight · Jan 1"),
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
      {GetFlightReservationEntityInstanceWithRandomGuid({
           .confirmation_code = u"ABC",
           .name = u"John Doe",
           // The departure time is set to 1 hour before the other
           // entity's departure time to ensure deterministic sorting,
           // as flight reservations suggestions are sorted by departure time.
           .departure_time = departure_time - base::Hours(1),
       }),
       GetFlightReservationEntityInstanceWithRandomGuid({
           .confirmation_code = u"DEF",
           .name = u"Bob Doe",
           .departure_time = departure_time,
       })});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  EXPECT_THAT(suggestions, TravelSuggestionsAre(HasLabel(u"Flight · John Doe"),
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
      {GetFlightReservationEntityInstanceWithRandomGuid({
           .flight_number = u"123",
           .ticket_number = u"ABC",
           // The departure time is set to 1 hour before the other
           // entity's departure time to ensure deterministic sorting,
           // as flight reservations suggestions are sorted by departure time.
           .departure_time = departure_time - base::Hours(1),
       }),
       GetFlightReservationEntityInstanceWithRandomGuid({
           .flight_number = u"234",
           .ticket_number = u"DEF",
           .departure_time = departure_time,
       })});
  SetForm({NAME_FULL, FLIGHT_RESERVATION_TICKET_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  EXPECT_THAT(suggestions, TravelSuggestionsAre(HasLabel(u"Flight · 123"),
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
  EXPECT_THAT(suggestions, TravelSuggestionsAre(HasIphFeature(kIphFeature)));
}

TEST_F(AutofillAiSuggestionGeneratorTest, ShowFetchingSuggestionWhenPending) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  SetForm({PASSPORT_NUMBER});
  SetEntities({});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)),
              IdentityDocSuggestionsAre(
                  EqualsSuggestion(SuggestionType::kFetchingAmbientData)));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       NoFetchingSuggestionWhenNoDataExists) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  SetForm({PASSPORT_NUMBER});
  SetEntities({});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(false));
  // We mock pending to ensure the test fails if the existence check is missing.
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
}

TEST_F(AutofillAiSuggestionGeneratorTest, NoFetchingSuggestionWhenNotPending) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  SetForm({PASSPORT_NUMBER});
  SetEntities({});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kSuccess));

  EXPECT_THAT(CreateAutofillAiFillingSuggestions(field(0)), IsEmpty());
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       NoFetchingSuggestionWhenTriggerFieldIsNotPending) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  // Form has Passport and National ID.
  SetForm({PASSPORT_NUMBER, NATIONAL_ID_CARD_NUMBER});

  // Passport has local data.
  SetEntities({test::GetPassportEntityInstance()});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kSuccess));

  // National ID is fetching, but the focused field (Passport) is not pending.
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kNationalIdCard)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kNationalIdCard)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  // User clicks Passport field.
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // Should contain Passport suggestions, but NOT fetching suggestion.
  EXPECT_THAT(suggestions, Not(IsEmpty()));
  EXPECT_THAT(
      suggestions,
      Not(Contains(EqualsSuggestion(SuggestionType::kFetchingAmbientData))));
}

class AutofillAiSuggestionGeneratorOrderShipmentTest
    : public AutofillAiSuggestionGeneratorTest {
 public:
  AutofillAiSuggestionGeneratorOrderShipmentTest()
      : AutofillAiSuggestionGeneratorTest(GetEnabledFeatures(), {}) {}

 private:
  static std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    std::vector<base::test::FeatureRef> features = GetDefaultEnabledFeatures();
    features.push_back(features::kAutofillAiOrder);
    features.push_back(features::kAutofillAiShipment);
    return features;
  }
};

TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_OrderFilteringByDomain) {
  // Create an Order entity for "example.com" and another for "other.com".
  SetEntities({
      test::GetOrderEntityInstance({
          .id = u"123",
          .merchant_domain = u"example.com",
          .guid = "00000000-0000-4000-8000-600000000001",
      }),
      test::GetOrderEntityInstance({
          .id = u"456",
          .merchant_domain = u"other.com",
          .guid = "00000000-0000-4000-8000-600000000002",
      }),
  });
  SetForm({ORDER_ID});

  // 1. Set page URL to "https://example.com/checkout".
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com/checkout"));

  std::vector<Suggestion> suggestions1 =
      CreateAutofillAiFillingSuggestions(field(0));

  // The order for "example.com" should be in the main menu, and "other.com"
  // in the fallback menu.
  EXPECT_THAT(suggestions1,
              ShoppingSuggestionsAre(
                  HasMainText(u"123"),
                  EqualsSuggestion(SuggestionType::kAutofillAiOtherOrders,
                                   l10n_util::GetStringUTF16(
                                       IDS_AUTOFILL_AI_OTHER_ORDERS))));

  // 2. Set page URL to "https://sub.other.com/checkout".
  client().set_last_committed_primary_main_frame_url(
      GURL("https://sub.other.com/checkout"));

  std::vector<Suggestion> suggestions2 =
      CreateAutofillAiFillingSuggestions(field(0));

  // The order for "other.com" (eTLD+1 match) should be in the main menu, and
  // "example.com" in the fallback menu.
  EXPECT_THAT(suggestions2,
              ShoppingSuggestionsAre(
                  HasMainText(u"456"),
                  EqualsSuggestion(SuggestionType::kAutofillAiOtherOrders,
                                   l10n_util::GetStringUTF16(
                                       IDS_AUTOFILL_AI_OTHER_ORDERS))));

  // 3. Set page URL to a site that doesn't match either (e.g.
  // "https://random.com").
  client().set_last_committed_primary_main_frame_url(
      GURL("https://random.com"));

  std::vector<Suggestion> suggestions3 =
      CreateAutofillAiFillingSuggestions(field(0));

  // Both orders should be in the fallback menu since neither matches
  // random.com.
  EXPECT_THAT(suggestions3,
              ShoppingSuggestionsAre(EqualsSuggestion(
                  SuggestionType::kAutofillAiOtherOrders,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_ORDERS))));
}

TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_ShipmentFilteringByDomain) {
  // Create a Shipment entity for "carrier.com" and another for
  // "other-carrier.com".
  SetEntities({
      test::GetShipmentEntityInstance({
          .tracking_number = u"TR123",
          .carrier_domain = u"carrier.com",
          .guid = "00000000-0000-4000-8000-700000000001",
      }),
      test::GetShipmentEntityInstance({
          .tracking_number = u"TR456",
          .carrier_domain = u"other-carrier.com",
          .guid = "00000000-0000-4000-8000-700000000002",
      }),
  });
  SetForm({SHIPMENT_TRACKING_NUMBER});

  // 1. Set page URL to "https://carrier.com/track".
  client().set_last_committed_primary_main_frame_url(
      GURL("https://carrier.com/track"));

  std::vector<Suggestion> suggestions1 =
      CreateAutofillAiFillingSuggestions(field(0));

  // The shipment for "carrier.com" should be in the main menu, and
  // "other-carrier.com" in the fallback menu.
  EXPECT_THAT(suggestions1,
              ShoppingSuggestionsAre(
                  EqualsSuggestion(SuggestionType::kFillAutofillAi, u"TR123"),
                  EqualsSuggestion(SuggestionType::kAutofillAiOtherShipments,
                                   l10n_util::GetStringUTF16(
                                       IDS_AUTOFILL_AI_OTHER_SHIPMENTS))));

  // 2. Set page URL to "https://sub.other-carrier.com/track".
  client().set_last_committed_primary_main_frame_url(
      GURL("https://sub.other-carrier.com/track"));

  std::vector<Suggestion> suggestions2 =
      CreateAutofillAiFillingSuggestions(field(0));

  // The shipment for "other-carrier.com" should be in the main menu, and
  // "carrier.com" in the fallback menu.
  EXPECT_THAT(suggestions2,
              ShoppingSuggestionsAre(
                  EqualsSuggestion(SuggestionType::kFillAutofillAi, u"TR456"),
                  EqualsSuggestion(SuggestionType::kAutofillAiOtherShipments,
                                   l10n_util::GetStringUTF16(
                                       IDS_AUTOFILL_AI_OTHER_SHIPMENTS))));

  // 3. Set page URL to "https://random.com".
  client().set_last_committed_primary_main_frame_url(
      GURL("https://random.com"));

  std::vector<Suggestion> suggestions3 =
      CreateAutofillAiFillingSuggestions(field(0));

  // Both shipments should be in the fallback menu since neither matches
  // random.com.
  EXPECT_THAT(suggestions3,
              ShoppingSuggestionsAre(EqualsSuggestion(
                  SuggestionType::kAutofillAiOtherShipments,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_SHIPMENTS))));
}

// Test that PersonalContext Order entities are sorted descending by order date,
// even if the older order has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_PersonalContextOrdering_OrderDate) {
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  EntityInstance order_recent = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"ORD_RECENT",
       .date = u"2026-07-01",
       .merchant_domain = u"example.com",
       .use_date = test::kJune2017 - base::Days(10),
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance order_old = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"ORD_OLD",
       .date = u"2025-01-01",
       .merchant_domain = u"example.com",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({order_old, order_recent});
  SetForm({ORDER_ID});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, ShoppingSuggestionsAre(HasMainText(u"ORD_RECENT"),
                                          HasMainText(u"ORD_OLD")));
}

// Test that PersonalContext Shipment entities are sorted descending by shipped
// date, even if the older shipment has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_PersonalContextOrdering_ShippedDate) {
  client().set_last_committed_primary_main_frame_url(
      GURL("https://carrier.com"));
  EntityInstance shipment_recent =
      test::GetShipmentEntityInstanceWithRandomGuid(
          {.tracking_number = u"TR_RECENT",
           .carrier_domain = u"carrier.com",
           .shipped_date = u"2026-07-01",
           .use_date = test::kJune2017 - base::Days(10),
           .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance shipment_old = test::GetShipmentEntityInstanceWithRandomGuid(
      {.tracking_number = u"TR_OLD",
       .carrier_domain = u"carrier.com",
       .shipped_date = u"2025-01-01",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({shipment_old, shipment_recent});
  SetForm({SHIPMENT_TRACKING_NUMBER});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(res, ShoppingSuggestionsAre(HasMainText(u"TR_RECENT"),
                                          HasMainText(u"TR_OLD")));
}

// Test that fallback suggestions (second-level children) follow the same
// ordering logic as primary suggestions. Specifically, PersonalContext Order
// entities in the fallback menu are sorted descending by order date, even if
// the older order has higher frecency.
TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_FallbackSuggestions_Ordering) {
  // Set the site domain to something that does not match either order domain,
  // so that both orders appear in the fallback menu.
  client().set_last_committed_primary_main_frame_url(
      GURL("https://random.com"));
  EntityInstance order_recent = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"ORD_RECENT",
       .date = u"2026-07-01",
       .merchant_domain = u"example.com",
       .use_date = test::kJune2017 - base::Days(10),
       .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance order_old = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"ORD_OLD",
       .date = u"2025-01-01",
       .merchant_domain = u"example.com",
       .use_date = test::kJune2017,
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({order_old, order_recent});
  SetForm({ORDER_ID});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      res, ShoppingSuggestionsAre(AllOf(
               SuggestionTypeHasTextAndAcceptability(
                   SuggestionType::kAutofillAiOtherOrders,
                   l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_ORDERS),
                   Suggestion::Acceptability::kSelectableButUnacceptable),
               ChildrenAre(
                   SuggestionTypeHasTextAndAcceptability(
                       SuggestionType::kFillAutofillAi, u"ORD_RECENT",
                       Suggestion::Acceptability::kSelectableAndAcceptable),
                   SuggestionTypeHasTextAndAcceptability(
                       SuggestionType::kFillAutofillAi, u"ORD_OLD",
                       Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

// Test that fallback suggestions (second-level children) follow the same
// deduplication logic as primary suggestions. When two fallback order entities
// would fill identical values, the higher-priority suggestion is kept and
// the lower-priority one is deduplicated.
TEST_F(AutofillAiSuggestionGeneratorOrderShipmentTest,
       GetFillingSuggestions_FallbackSuggestions_Deduplication) {
  client().set_last_committed_primary_main_frame_url(
      GURL("https://random.com"));
  EntityInstance order_server = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"123",
       .merchant_domain = u"example.com",
       .record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance pcontext = test::GetOrderEntityInstanceWithRandomGuid(
      {.id = u"123",
       .merchant_domain = u"example.com",
       .record_type = EntityInstance::RecordType::kPersonalContext});

  SetEntities({pcontext, order_server});
  SetForm({ORDER_ID});

  std::vector<Suggestion> res = CreateAutofillAiFillingSuggestions(field(0));
  // Since `order_pcontext` is a subset/duplicate of `order_server`, only one
  // child suggestion should be generated in the fallback menu.
  EXPECT_THAT(res,
              ShoppingSuggestionsAre(AllOf(
                  SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kAutofillAiOtherOrders,
                      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_ORDERS),
                      Suggestion::Acceptability::kSelectableButUnacceptable),
                  ChildrenAre(SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kFillAutofillAi, u"123",
                      Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

class AutofillAiSuggestionGeneratorSplitManageSuggestionTest
    : public AutofillAiSuggestionGeneratorTest {
 public:
  AutofillAiSuggestionGeneratorSplitManageSuggestionTest()
      : AutofillAiSuggestionGeneratorTest(GetEnabledFeatures(),
                                          /*disabled_features=*/{}) {}

 private:
  static std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    auto features = GetDefaultEnabledFeatures();
    features.push_back(
        features::kSuggestionManageButtonSplitForEnhancedAutofill);
    features.push_back(features::kAutofillAiOrder);
    features.push_back(features::kAutofillAiShipment);
    return features;
  }
};

TEST_F(AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
       SuggestionsFooterContainsManageAutofillAiIdentityDocsSuggestion) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kFillAutofillAi),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(
                              SuggestionType::kManageAutofillAiIdentityDocs)));
}

TEST_F(AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
       SuggestionsFooterContainsManageAutofillAiTravelSuggestion) {
  SetEntities({test::GetVehicleEntityInstanceWithRandomGuid()});
  SetForm({VEHICLE_LICENSE_PLATE});
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsSuggestion(SuggestionType::kFillAutofillAi),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAutofillAiTravel)));
}

TEST_F(AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
       SuggestionsFooterContainsManageAutofillAiShoppingSuggestion) {
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SetEntities({test::GetOrderEntityInstanceWithRandomGuid()});
  SetForm({ORDER_ID});
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsSuggestion(SuggestionType::kFillAutofillAi),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAutofillAiShopping)));
}

TEST_F(
    AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
    SuggestionsFooterContainsManageAutofillAiSuggestionWhenMultipleSectionsPresent) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid(),
               test::GetVehicleEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER, NAME_FULL, VEHICLE_LICENSE_PLATE});
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kFillAutofillAi),
                          EqualsSuggestion(SuggestionType::kFillAutofillAi),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageAutofillAi)));
}

TEST_F(AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
       ShowFetchingSuggestionWhenPending) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  SetForm({PASSPORT_NUMBER});
  SetEntities({});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(0)),
      ElementsAre(
          EqualsSuggestion(SuggestionType::kFetchingAmbientData),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kManageAutofillAiIdentityDocs)));
}

TEST_F(AutofillAiSuggestionGeneratorSplitManageSuggestionTest,
       ShowFetchingSuggestionWhenMultiplePending) {
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager> access_manager;
  client().set_personal_context_access_manager(&access_manager);

  SetForm({PASSPORT_NUMBER, VEHICLE_LICENSE_PLATE, NAME_FULL});
  SetEntities({});

  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  // The triggering field (`NAME_FULL`) is compatible with multiple entity
  // types. The suggestion generator checks the status of all compatible
  // types, so we define catch-all default expectations first to prevent
  // GMock from reporting unexpected call failures on other types.
  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType)
      .WillRepeatedly(Return(RequestStatus::kNotStarted));

  EXPECT_CALL(access_manager, ServerHasSpiiPresenceSignal(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kPassport)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  EXPECT_CALL(access_manager,
              ServerHasSpiiPresenceSignal(EntityType(EntityTypeName::kVehicle)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(access_manager, GetPrefetchStatusByEntityType(
                                  EntityType(EntityTypeName::kVehicle)))
      .WillRepeatedly(Return(RequestStatus::kPending));

  EXPECT_THAT(
      CreateAutofillAiFillingSuggestions(field(2)),
      ElementsAre(EqualsSuggestion(SuggestionType::kFetchingAmbientData),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAutofillAi)));
}

class AutofillAiSuggestionGeneratorPolicyTest
    : public AutofillAiSuggestionGeneratorTest {
 public:
  AutofillAiSuggestionGeneratorPolicyTest()
      : AutofillAiSuggestionGeneratorTest(GetEnabledFeatures(),
                                          /*disabled_features=*/{}) {}

 private:
  static std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    auto features = GetDefaultEnabledFeatures();
    features.push_back(
        features::kAutofillEnableAutofillSettingsEnterprisePolicy);
    return features;
  }
};

// Tests that identity document entities are blocked from suggestions when the
// kIdentityDocs policy category is blocked.
TEST_F(AutofillAiSuggestionGeneratorPolicyTest, BlockIdentityDocs) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kIdentityDocs, true);

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_TRUE(suggestions.empty());
}

// Tests that travel entities are blocked from suggestions when the kTravel
// policy category is blocked.
TEST_F(AutofillAiSuggestionGeneratorPolicyTest, BlockTravel) {
  SetEntities({test::GetVehicleEntityInstanceWithRandomGuid()});
  SetForm({VEHICLE_LICENSE_PLATE});

  client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kTravel, true);

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_TRUE(suggestions.empty());
}

// Tests that the "Other orders" suggestion is correctly generated when there
// are fallback order entities. It verifies the hierarchy, acceptability, and
// that prefix filtering is NOT applied to fallback orders.
TEST_F(AutofillAiSuggestionGeneratorTest, GeneratesOtherOrdersSuggestion) {
  // Setup: 3 order entities with different domains.
  // `order_a` matches the page domain (`amazon.com`), so it will be in the main
  // suggested list. `order_b` and `order_c` belong to different domains, so
  // they should be fallback options.
  EntityInstance order_a = GetOrderEntityInstance({
      .merchant_name = u"Amazon",
      .merchant_domain = u"amazon.com",
      .guid = "00000000-0000-4000-8000-600000000001",
      .use_count = 10,
  });
  EntityInstance order_b = GetOrderEntityInstance({
      .merchant_name = u"BestBuy",
      .merchant_domain = u"bestbuy.com",
      .guid = "00000000-0000-4000-8000-600000000002",
      .use_count = 5,
  });
  EntityInstance order_c = GetOrderEntityInstance({
      .merchant_name = u"Costco",
      .merchant_domain = u"costco.com",
      .guid = "00000000-0000-4000-8000-600000000003",
      .use_count = 1,
  });

  SetEntities({order_a, order_b, order_c});
  SetForm({ORDER_ID, ORDER_MERCHANT_NAME});
  client().set_last_committed_primary_main_frame_url(
      GURL("https://amazon.com/checkout"));

  // Generate suggestions.
  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  // Expected layout:
  // 1. The top-level suggestion for Order A (Amazon).
  // 2. The "Other orders" parent suggestion (`kAutofillAiOtherOrders`),
  //    containing BestBuy and Costco as children.
  // 3. The footer separator and manage suggestions.
  EXPECT_THAT(
      suggestions,
      ShoppingSuggestionsAre(
          SuggestionTypeHasTextAndAcceptability(
              SuggestionType::kFillAutofillAi, u"Amazon",
              Suggestion::Acceptability::kSelectableAndAcceptable),
          AllOf(
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kAutofillAiOtherOrders,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_OTHER_ORDERS),
                  Suggestion::Acceptability::kSelectableButUnacceptable),
              ChildrenAre(
                  SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kFillAutofillAi, u"BestBuy",
                      Suggestion::Acceptability::kSelectableAndAcceptable),
                  SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kFillAutofillAi, u"Costco",
                      Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

// Tests that when there are no primary order suggestions (e.g. no orders
// match the current site's domain), fallback order suggestions are still
// generated and the menu is labeled "All orders"
// (IDS_AUTOFILL_AI_ALL_ORDERS).
TEST_F(AutofillAiSuggestionGeneratorTest,
       GeneratesAllOrdersSuggestion_NoPrimaryOrders) {
  EntityInstance order_bestbuy = GetOrderEntityInstance({
      .merchant_name = u"BestBuy",
      .merchant_domain = u"bestbuy.com",
      .guid = "00000000-0000-4000-8000-600000000002",
      .use_count = 5,
  });
  EntityInstance order_costco = GetOrderEntityInstance({
      .merchant_name = u"Costco",
      .merchant_domain = u"costco.com",
      .guid = "00000000-0000-4000-8000-600000000003",
      .use_count = 1,
  });

  SetEntities({order_bestbuy, order_costco});
  SetForm({ORDER_ID, ORDER_MERCHANT_NAME});
  client().set_last_committed_primary_main_frame_url(
      GURL("https://amazon.com/checkout"));

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(1));

  // Expected layout:
  // 1. The fallback parent suggestion (`kAutofillAiOtherOrders`), labeled
  //    "All orders", containing BestBuy and Costco as children.
  // 2. The footer separator and manage suggestions.
  EXPECT_THAT(
      suggestions,
      ShoppingSuggestionsAre(AllOf(
          SuggestionTypeHasTextAndAcceptability(
              SuggestionType::kAutofillAiOtherOrders,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_ORDERS),
              Suggestion::Acceptability::kSelectableButUnacceptable),
          ChildrenAre(
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kFillAutofillAi, u"BestBuy",
                  Suggestion::Acceptability::kSelectableAndAcceptable),
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kFillAutofillAi, u"Costco",
                  Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

// Tests that the "Other shipments" suggestion is correctly generated when there
// are fallback shipment entities alongside primary shipment entities.
TEST_F(AutofillAiSuggestionGeneratorTest, GeneratesOtherShipmentsSuggestion) {
  // Setup: 3 shipment entities with different domains.
  EntityInstance shipment_a = test::GetShipmentEntityInstance({
      .tracking_number = u"TR123",
      .carrier_domain = u"carrier.com",
      .guid = "00000000-0000-4000-8000-700000000001",
  });
  EntityInstance shipment_b = test::GetShipmentEntityInstance({
      .tracking_number = u"TR456",
      .carrier_domain = u"other-carrier.com",
      .guid = "00000000-0000-4000-8000-700000000002",
  });
  EntityInstance shipment_c = test::GetShipmentEntityInstance({
      .tracking_number = u"TR789",
      .carrier_domain = u"third-carrier.com",
      .guid = "00000000-0000-4000-8000-700000000003",
  });

  SetEntities({shipment_a, shipment_b, shipment_c});
  SetForm({SHIPMENT_TRACKING_NUMBER});
  client().set_last_committed_primary_main_frame_url(
      GURL("https://carrier.com/track"));

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // Expected layout:
  // 1. The top-level suggestion for Shipment A (carrier.com).
  // 2. The "Other shipments" parent suggestion (`kAutofillAiOtherShipments`),
  //    containing TR456 and TR789 as children.
  // 3. The footer separator and manage suggestions.
  EXPECT_THAT(
      suggestions,
      ShoppingSuggestionsAre(
          SuggestionTypeHasTextAndAcceptability(
              SuggestionType::kFillAutofillAi, u"TR123",
              Suggestion::Acceptability::kSelectableAndAcceptable),
          AllOf(
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kAutofillAiOtherShipments,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_OTHER_SHIPMENTS),
                  Suggestion::Acceptability::kSelectableButUnacceptable),
              ChildrenAre(
                  SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kFillAutofillAi, u"TR456",
                      Suggestion::Acceptability::kSelectableAndAcceptable),
                  SuggestionTypeHasTextAndAcceptability(
                      SuggestionType::kFillAutofillAi, u"TR789",
                      Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

// Tests that when there are no primary shipment suggestions, fallback shipment
// suggestions are still generated and the menu is labeled "All shipments"
// (`IDS_AUTOFILL_AI_ALL_SHIPMENTS`).
TEST_F(AutofillAiSuggestionGeneratorTest,
       GeneratesAllShipmentsSuggestion_NoPrimaryShipments) {
  EntityInstance shipment_a = test::GetShipmentEntityInstance({
      .tracking_number = u"TR456",
      .carrier_domain = u"other-carrier.com",
      .guid = "00000000-0000-4000-8000-700000000002",
  });
  EntityInstance shipment_b = test::GetShipmentEntityInstance({
      .tracking_number = u"TR789",
      .carrier_domain = u"third-carrier.com",
      .guid = "00000000-0000-4000-8000-700000000003",
  });

  SetEntities({shipment_a, shipment_b});
  SetForm({SHIPMENT_TRACKING_NUMBER});
  client().set_last_committed_primary_main_frame_url(
      GURL("https://random.com"));

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));

  // Expected layout:
  // 1. The fallback parent suggestion (`kAutofillAiOtherShipments`), labeled
  //    "All shipments", containing TR456 and TR789 as children.
  // 2. The footer separator and manage suggestions.
  EXPECT_THAT(
      suggestions,
      ShoppingSuggestionsAre(AllOf(
          SuggestionTypeHasTextAndAcceptability(
              SuggestionType::kAutofillAiOtherShipments,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_ALL_SHIPMENTS),
              Suggestion::Acceptability::kSelectableButUnacceptable),
          ChildrenAre(
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kFillAutofillAi, u"TR456",
                  Suggestion::Acceptability::kSelectableAndAcceptable),
              SuggestionTypeHasTextAndAcceptability(
                  SuggestionType::kFillAutofillAi, u"TR789",
                  Suggestion::Acceptability::kSelectableAndAcceptable)))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeShownWhenambientAutofillNoticeNeverShown) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice)));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeShownWhenGeminiAckedLongEnoughAgo) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  client().GetPrefs()->SetTime(
      prefs::kAutofillAiPrivateInferenceNoticeFirstShownTimestamp,
      base::Time::Now() - base::Days(10));
  // Because the acked was done 7 days ago, creating private inference notice
  // suggestion is allowed.
  client().GetPrefs()->SetTime(
      prefs::kAmbientAutofillNoticeAcknowledgedTimestamp,
      base::Time::Now() - base::Days(7));

  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice)));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeNotShownWhenAmbientAutofillShownButNotAcked) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  client().GetPrefs()->SetTime(
      prefs::kAutofillAiPrivateInferenceNoticeFirstShownTimestamp,
      base::Time::Now());

  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Not(Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeNotShownWhenAmbientAutofillAckedTooRecently) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  client().GetPrefs()->SetTime(
      prefs::kAutofillAiPrivateInferenceNoticeFirstShownTimestamp,
      base::Time::Now() - base::Days(2));
  // Because the acked was done 1 day ago, creating private inference notice
  // suggestion is not allowed.
  client().GetPrefs()->SetTime(
      prefs::kAmbientAutofillNoticeAcknowledgedTimestamp,
      base::Time::Now() - base::Days(1));

  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Not(Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeNotShownWhenPrivateInferenceNoticeAcked) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  client().GetPrefs()->SetTime(
      prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
      base::Time::Now());

  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Not(Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeNotShownWhenFeatureDisabled) {
  SetEntities({GetPassportEntityInstanceWithRandomGuid()});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Not(Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice))));
}

TEST_F(AutofillAiSuggestionGeneratorTest,
       PrivateInferenceNoticeShownEvenWhenNoEntitiesExist) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillAiUsePrivateAi);
  SetEntities({});
  SetForm({PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(field(0));
  EXPECT_THAT(suggestions,
              Contains(EqualsSuggestion(
                  SuggestionType::kAutofillAiPrivateInferenceNotice)));
}

}  // namespace
}  // namespace autofill
