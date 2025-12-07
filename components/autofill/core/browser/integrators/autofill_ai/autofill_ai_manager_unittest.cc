// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager_test_api.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/389629573): Refactor this test to handle only the
// implementation under `features::kAutofillAiWithDataSchema`
// flag.
namespace autofill {
namespace {

using enum SuggestionType;
using AutofillAiPayload = Suggestion::AutofillAiPayload;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pointer;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::Truly;
using ::testing::VariantWith;

constexpr auto kAcceptBubble =
    AutofillClient::AutofillAiBubbleClosedReason::kAccepted;
constexpr auto kDeclineBubble =
    AutofillClient::AutofillAiBubbleClosedReason::kClosed;
constexpr auto kIgnoreBubble =
    AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted;

auto FirstElementIs(auto&& matcher) {
  return ResultOf(
      "first element", [](const auto& container) { return *container.begin(); },
      std::move(matcher));
}

auto HasType(SuggestionType expected_type) {
  return Field("Suggestion::type", &Suggestion::type, Eq(expected_type));
}

auto HasAutofillAiPayload(auto expected_payload) {
  return Field("Suggestion::payload", &Suggestion::payload,
               VariantWith<AutofillAiPayload>(expected_payload));
}

auto HasAttributeWithValue(AttributeType attribute_type,
                           std::u16string value,
                           const std::string& app_locale) {
  return Truly([=](const EntityInstance& entity) {
    if (entity.type() != attribute_type.entity_type()) {
      return false;
    }
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(attribute_type);
    return attribute && attribute->GetCompleteInfo(app_locale) == value;
  });
}

auto PassportWithNumber(std::u16string number) {
  return HasAttributeWithValue(
      AttributeType(AttributeTypeName::kPassportNumber), std::move(number),
      /*app_locale=*/"");
}

auto VehicleWithLicensePlate(std::u16string license_plate) {
  return HasAttributeWithValue(
      AttributeType(AttributeTypeName::kVehiclePlateNumber),
      std::move(license_plate), /*app_locale=*/"");
}

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ShowEntityImportBubble,
              (EntityInstance entity,
               std::optional<EntityInstance> old_entity,
               EntityImportPromptResultCallback prompt_acceptance_callback),
              (override));
  MOCK_METHOD(void,
              TriggerAutofillAiSavePromptSurvey,
              (bool prompt_accepted,
               EntityType entity_type,
               const base::flat_set<EntityTypeName>& saved_entities),
              (override));
  MOCK_METHOD(void,
              TriggerAutofillAiFillingJourneySurvey,
              (bool suggestion_accepted,
               EntityType type,
               const base::flat_set<EntityTypeName>& saved_entities,
               const FieldTypeSet& triggering_field_types),
              (override));
};
class AutofillAiManagerTest : public testing::Test {
 public:
  AutofillAiManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiServerModel},
        /*disabled_features=*/{});
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    autofill_client().SetUpPrefsAndIdentityForAutofillAi();
    autofill_client().set_sync_service(&sync_service_);
    autofill_client().GetSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPayments, true);
  }

  std::u16string GetValueFromEntity(const EntityInstance entity,
                                    AttributeType attribute,
                                    const std::string& app_locale = "") {
    return entity.attribute(attribute)->GetCompleteInfo(app_locale);
  }

  std::u16string GetValueFromEntityForAttributeTypeName(
      const EntityInstance entity,
      AttributeTypeName type,
      const std::string& app_locale) {
    AttributeType attribute_type = AttributeType(type);
    base::optional_ref<const AttributeInstance> instance =
        entity.attribute(attribute_type);
    CHECK(instance);
    return instance->GetInfo(attribute_type.field_type(), app_locale,
                             /*format_string=*/std::nullopt);
  }

  // Given a `FormStructure` sets `field_types_predictions` for each field in
  // the form.
  void AddPredictionsToFormStructure(
      FormStructure& form_structure,
      const std::vector<std::vector<FieldType>>& field_types_predictions) {
    CHECK_EQ(form_structure.field_count(), field_types_predictions.size());
    for (size_t i = 0; i < form_structure.field_count(); i++) {
      std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                      FieldPrediction>
          predictions_for_field;
      for (FieldType type : field_types_predictions[i]) {
        AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
            prediction;
        prediction.set_type(type);
        predictions_for_field.push_back(prediction);
      }

      form_structure.field(i)->set_server_predictions(predictions_for_field);
    }
  }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    edm().AddOrUpdateEntityInstance(std::move(entity));
    webdata_helper_.WaitUntilIdle();
  }

  void RemoveEntityInstance(EntityInstance::EntityId guid) {
    edm().RemoveEntityInstance(guid);
    webdata_helper_.WaitUntilIdle();
  }

  void AddAutofillProfile() {
    autofill_client_.GetPersonalDataManager().address_data_manager().AddProfile(
        test::GetFullProfile());
  }

  base::span<const EntityInstance> GetEntityInstances() {
    webdata_helper_.WaitUntilIdle();
    return edm().GetEntityInstances();
  }

  MockAutofillClient& autofill_client() { return autofill_client_; }
  EntityDataManager& edm() { return *autofill_client().GetEntityDataManager(); }
  AutofillAiManager& manager() { return manager_; }
  TestStrikeDatabase& strike_database() { return strike_database_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  syncer::TestSyncService sync_service_;
  NiceMock<MockAutofillClient> autofill_client_;
  TestStrikeDatabase strike_database_;
  AutofillAiManager manager_{&autofill_client(), &strike_database_};
};

// Tests that the user receives a filling suggestion when interacting with
// a field that has AutofillAi predictions.
TEST_F(AutofillAiManagerTest,
       GetSuggestionsTriggeringFieldIsAutofillAi_ReturnFillingSuggestion) {
  test::FormDescription form_description = {
      .fields = {{.role = PASSPORT_NUMBER}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});

  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  EXPECT_THAT(manager().GetSuggestions(form_structure, form.fields().front()),
              ElementsAre(HasType(kFillAutofillAi), HasType(kSeparator),
                          HasType(kManageAutofillAi)));
}

// Tests that IPH should be displayed if the user is opted out of the feature,
// has an address, and form submission with filled out fields would lead to
// entity import.
TEST_F(AutofillAiManagerTest, ShouldDisplayIph) {
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});
  AddAutofillProfile();
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  EXPECT_TRUE(
      manager().ShouldDisplayIph(form_structure, form.fields()[0].global_id()));
}

// Tests that IPH should not be displayed if the user is opted out of the
// feature, but does not have address or payments data stored.
TEST_F(AutofillAiManagerTest,
       ShouldNotDisplayIphWhenUserHasNoAddressOrPaymentsData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAiIgnoreWhetherUserHasAddressOrPaymentsDataForIph);
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  EXPECT_FALSE(
      manager().ShouldDisplayIph(form_structure, form.fields()[0].global_id()));
}

// Tests that IPH is not displayed if the user has disabled, say,
// identity-related entities, and the submitted form would import (only) an
// identity-related entity.
TEST_F(AutofillAiManagerTest,
       ShouldNotDisplayIphWhenUserHasDisabledTheGroupOfEntities) {
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  FieldGlobalId field_id = form.fields()[0].global_id();
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});
  AddAutofillProfile();
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiTravelEntitiesEnabled, false);
  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiIdentityEntitiesEnabled, false);
  EXPECT_FALSE(manager().ShouldDisplayIph(form_structure, field_id));

  // Confirm that it is only the pref for identity entities that affects IPH.
  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiIdentityEntitiesEnabled, true);
  EXPECT_TRUE(manager().ShouldDisplayIph(form_structure, field_id));
}

// Tests that if kAutofillAiIgnoreWhetherUserHasAddressOrPaymentsDataForIph is
// enabled, IPH should be displayed when the user is opted out of the feature
// and does not have address or payments data stored.
TEST_F(AutofillAiManagerTest,
       ShouldDisplayIphWhenUserHasNoAddressOrPaymentsDataAndFeatureFlagIsOn) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiIgnoreWhetherUserHasAddressOrPaymentsDataForIph};
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  EXPECT_TRUE(
      manager().ShouldDisplayIph(form_structure, form.fields()[0].global_id()));
}

// Tests that IPH should not be displayed if the user is opted into AutofillAI
// already.
TEST_F(AutofillAiManagerTest, ShouldNotDisplayIphWhenOptedIn) {
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_NUMBER}});
  AddAutofillProfile();
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedIn);

  EXPECT_FALSE(
      manager().ShouldDisplayIph(form_structure, form.fields()[0].global_id()));
}

// Tests that IPH should not be displayed if the page does not contain enough
// information for an import.
TEST_F(AutofillAiManagerTest,
       ShouldNotDisplayIphWhenInsufficientDataForImport) {
  test::FormDescription form_description = {.fields = {{}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure, {{PASSPORT_ISSUE_DATE}});
  AddAutofillProfile();
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  EXPECT_FALSE(
      manager().ShouldDisplayIph(form_structure, form.fields()[0].global_id()));
}

// Tests that IPH is not displayed on a field without AutofillAI predictions.
TEST_F(AutofillAiManagerTest, ShouldNotDisplayIphOnUnrelatedField) {
  test::FormDescription form_description = {.fields = {{}, {}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(
      form_structure, {{PASSPORT_NUMBER}, {PHONE_HOME_CITY_AND_NUMBER}});
  AddAutofillProfile();
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);

  EXPECT_FALSE(
      manager().ShouldDisplayIph(form_structure, form.fields()[1].global_id()));
}

TEST_F(AutofillAiManagerTest,
       FillingMomentSurvey_SuggestionAccepted_ShowSurvey) {
  EntityInstance passport_entity = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport_entity);

  test::FormDescription form_description = {.fields = {{}, {}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure,
                                {{NAME_FULL}, {PASSPORT_NUMBER}});
  form_structure.field(0)->set_value(GetValueFromEntityForAttributeTypeName(
      passport_entity, AttributeTypeName::kPassportName, /*app_locale=*/""));
  form_structure.field(1)->set_value(GetValueFromEntityForAttributeTypeName(
      passport_entity, AttributeTypeName::kPassportNumber, /*app_locale=*/""));

  Suggestion passport_suggestion(SuggestionType::kFillAutofillAi);
  passport_suggestion.payload =
      Suggestion::AutofillAiPayload(passport_entity.guid());
  manager().OnSuggestionsShown(form_structure, *form_structure.field(0),
                               {passport_suggestion}, {});
  manager().OnDidFillSuggestion(passport_entity, form_structure,
                                *form_structure.field(0),
                                /*filled_fiekds*/ {}, {});

  EXPECT_CALL(
      autofill_client(),
      TriggerAutofillAiFillingJourneySurvey(
          /*suggestion_accepted=*/true, passport_entity.type(),
          /*saved_entities=*/
          base::flat_set<EntityTypeName>({EntityTypeName::kPassport}),
          /*triggering_field_types=*/
          FieldTypeSet({AutofillType(NAME_FULL).GetAutofillAiTypes()})));
  ASSERT_FALSE(manager().OnFormSubmitted(form_structure, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerTest,
       FillingMomentSurvey_SuggestionDeclined_ShowSurvey) {
  EntityInstance passport_entity = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport_entity);

  test::FormDescription form_description = {.fields = {{}, {}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure,
                                {{NAME_FULL}, {PASSPORT_NUMBER}});
  form_structure.field(0)->set_value(GetValueFromEntityForAttributeTypeName(
      passport_entity, AttributeTypeName::kPassportName, /*app_locale=*/""));
  form_structure.field(1)->set_value(GetValueFromEntityForAttributeTypeName(
      passport_entity, AttributeTypeName::kPassportNumber, /*app_locale=*/""));

  Suggestion passport_suggestion(SuggestionType::kFillAutofillAi);
  passport_suggestion.payload =
      Suggestion::AutofillAiPayload(passport_entity.guid());
  manager().OnSuggestionsShown(form_structure, *form_structure.field(0),
                               {passport_suggestion}, {});

  EXPECT_CALL(
      autofill_client(),
      TriggerAutofillAiFillingJourneySurvey(
          /*suggestion_accepted=*/false, passport_entity.type(),
          /*saved_entities=*/
          base::flat_set<EntityTypeName>({EntityTypeName::kPassport}),
          /*triggering_field_types=*/
          FieldTypeSet({AutofillType(NAME_FULL).GetAutofillAiTypes()})));
  ASSERT_FALSE(manager().OnFormSubmitted(form_structure, /*ukm_source_id=*/{}));
}

// Tests that surveys are only shown if no save (or update) prompts are shown.
TEST_F(
    AutofillAiManagerTest,
    FillingMomentSurvey_SuggestionAccepted_SavePromptShown_SurveyIsNotShown) {
  EntityInstance passport_entity = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport_entity);

  test::FormDescription form_description = {.fields = {{}, {}}};
  FormData form = test::GetFormData(form_description);
  FormStructure form_structure = FormStructure(form);
  AddPredictionsToFormStructure(form_structure,
                                {{NAME_FULL}, {PASSPORT_NUMBER}});
  form_structure.field(0)->set_value(GetValueFromEntityForAttributeTypeName(
      passport_entity, AttributeTypeName::kPassportName, /*app_locale=*/""));
  // Fill the passport number with a different value to trigger a save prompt
  // survey.
  form_structure.field(1)->set_value(u"12345");

  Suggestion passport_suggestion(SuggestionType::kFillAutofillAi);
  passport_suggestion.payload =
      Suggestion::AutofillAiPayload(passport_entity.guid());
  manager().OnSuggestionsShown(form_structure, *form_structure.field(0),
                               {passport_suggestion}, {});
  manager().OnDidFillSuggestion(passport_entity, form_structure,
                                *form_structure.field(0),
                                /*filled_fiekds*/ {}, {});

  EXPECT_CALL(autofill_client(), TriggerAutofillAiFillingJourneySurvey)
      .Times(0);
  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity)));
  EXPECT_TRUE(manager().OnFormSubmitted(form_structure, /*ukm_source_id=*/{}));
  ASSERT_FALSE(old_entity);
  ASSERT_TRUE(new_entity);
}

class AutofillAiManagerImportFormTest : public AutofillAiManagerTest {
 public:
  AutofillAiManagerImportFormTest() = default;

  static constexpr char kDefaultUrl[] = "https://example.com";
  static constexpr char16_t kDefaultVehicleOwner[] = u"Jane Doe";
  static constexpr char16_t kDefaultPassportNumber[] = u"123";
  static constexpr char16_t kDefaultLicensePlate[] = u"XC-12-34";
  static constexpr char16_t kOtherLicensePlate[] = u"567435";

  [[nodiscard]] std::unique_ptr<FormStructure> CreateFormStructure(
      const std::vector<FieldType>& field_types_predictions,
      std::string url = std::string(kDefaultUrl)) {
    test::FormDescription form_description{.url = std::move(url)};
    for (FieldType field_type : field_types_predictions) {
      form_description.fields.emplace_back(
          test::FieldDescription({.role = field_type}));
    }
    auto form_structure =
        std::make_unique<FormStructure>(test::GetFormData(form_description));
    for (size_t i = 0; i < form_structure->field_count(); i++) {
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
          prediction;
      prediction.set_type(form_description.fields[i].role);
      form_structure->field(i)->set_server_predictions({prediction});
    }
    return form_structure;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreatePassportForm(
      std::u16string passport_number = std::u16string(kDefaultPassportNumber),
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form = CreateFormStructure(
        {NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER}, std::move(url));
    form->field(0)->set_value(u"Jon Doe");
    form->field(1)->set_value(std::move(passport_number));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateVehicleForm(
      std::u16string license_plate = std::u16string(kDefaultLicensePlate),
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form =
        CreateFormStructure({NAME_FULL, VEHICLE_LICENSE_PLATE}, std::move(url));
    form->field(0)->set_value(kDefaultVehicleOwner);
    form->field(1)->set_value(std::move(license_plate));
    return form;
  }

  std::u16string GetValueFromEntity(const EntityInstance entity,
                                    AttributeType attribute,
                                    const std::string& app_locale = "") {
    return entity.attribute(attribute)->GetCompleteInfo(app_locale);
  }

  std::u16string GetValueFromEntityForAttributeTypeName(
      const EntityInstance entity,
      AttributeTypeName type,
      const std::string& app_locale) {
    AttributeType attribute_type = AttributeType(type);
    base::optional_ref<const AttributeInstance> instance =
        entity.attribute(attribute_type);
    CHECK(instance);
    return instance->GetInfo(attribute_type.field_type(), app_locale,
                             /*format_string=*/std::nullopt);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWalletVehicleRegistration};
};

// Tests that save prompts are only shown three times per url and entity type.
TEST_F(AutofillAiManagerImportFormTest, StrikesForSavePromptsPerUrl) {
  constexpr char16_t kOtherPassportNumber[] = u"67867";

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
  }

  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));
  // The fourth attempt is ignored.
  EXPECT_FALSE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));

  // But submitting on a different page leads to a prompt.
  check.Call();
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kDefaultPassportNumber, "https://other.com"),
      /*ukm_source_id=*/{}));
  // And so does submitting a different entity type on the original page.
  EXPECT_TRUE(
      manager().OnFormSubmitted(*CreateVehicleForm(), /*ukm_source_id=*/{}));
}

// Tests that save prompts are only shown three times per strike attribute (in
// this case, passport number).
TEST_F(AutofillAiManagerImportFormTest, StrikesForSavePromptsPerAttribute) {
  constexpr char16_t kOtherPassportNumber[] = u"567435";

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kIgnoreBubble));
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(3)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
  }

  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  // The next attempt is ignored, even though it is a different domain.
  EXPECT_FALSE(manager().OnFormSubmitted(
      *CreatePassportForm(kDefaultPassportNumber, "https://other.com"),
      /*ukm_source_id*/ {}));

  // But submitting a different passport number leads to a prompt.
  check.Call();
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber, "https://other.com"),
      /*ukm_source_id*/ {}));
}

// Tests that migration prompts are only shown three times per strike attribute
// (in this case, passport number).
TEST_F(AutofillAiManagerImportFormTest,
       StrikesForMigrationPromptsPerAttribute) {
  EntityInstance local_vehicle_entity_default_plate =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner, .plate = kDefaultLicensePlate});
  // A different vin number is also needed because it is part of the entity
  // strike keys.
  EntityInstance local_vehicle_entity_other_plate_and_vin =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner,
           .plate = kOtherLicensePlate,
           .number = u"123"});
  AddOrUpdateEntityInstance(local_vehicle_entity_default_plate);
  AddOrUpdateEntityInstance(local_vehicle_entity_other_plate_and_vin);

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kIgnoreBubble));
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(3)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kOtherLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
  }

  std::unique_ptr<FormStructure> form = CreateVehicleForm();
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // The next attempt is ignored, even though it is a different domain.
  std::unique_ptr<FormStructure> different_domain =
      CreateVehicleForm(kDefaultLicensePlate, "https://other.com");
  EXPECT_FALSE(manager().OnFormSubmitted(*different_domain,
                                         /*ukm_source_id*/ {}));

  // But submitting a different license plate and vin number leads to a prompt.
  std::unique_ptr<FormStructure> different_plate_form =
      CreateVehicleForm(kOtherLicensePlate, "https://other.com");
  check.Call();
  EXPECT_TRUE(manager().OnFormSubmitted(*different_plate_form,
                                        /*ukm_source_id*/ {}));
}

// Tests that update prompts are only shown three times per entity that is to
// be updated. Tests that accepting a prompt resets the counter.
TEST_F(AutofillAiManagerImportFormTest, StrikesForUpdates) {
  constexpr char16_t kOtherPassportNumber[] = u"67867";
  constexpr char16_t kOtherPassportNumber2[] = u"6785634567";

  {
    InSequence s;
    // Accept the first prompt.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // Accept the third prompt.
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // If the user just ignores the prompt, no strikes are recorded.
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber2), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kIgnoreBubble));

    // Only three more prompts will be shown for the next update because the
    // user declines explicitly.
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber2), _, _))
        .Times(3)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
  }

  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  // The first dialog was accepted.
  EXPECT_THAT(GetEntityInstances(), SizeIs(1));

  // Simulate three submissions - the last prompt is accepted.
  ASSERT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));

  // Simulate four more submissions - only three prompts are shown.
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
  EXPECT_FALSE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber2), /*ukm_source_id=*/{}));
}

// Tests that accepting a save prompt for an entity resets the strike counter
// for that entity type.
TEST_F(AutofillAiManagerImportFormTest, AcceptingResetsStrikesPerUrl) {
  constexpr char16_t kOtherPassportNumber[] = u"56745";
  constexpr char16_t kOtherLicensePlate[] = u"MU-LJ-4500";

  MockFunction<void()> check;
  {
    InSequence s;
    // First, we expect to see two save attempts for a passport and two save
    // attempts for a vehicle.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);

    // We accept the next save prompt for a passport form.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // We now only get one more vehicle save prompt (despite submitting a form
    // twice), but two more passport prompts because passport strikes were
    // reset.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kOtherLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
    EXPECT_CALL(
        autofill_client(),
        ShowEntityImportBubble(PassportWithNumber(kOtherPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
  }

  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreateVehicleForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreateVehicleForm(), /*ukm_source_id=*/{}));
  check.Call();

  // This one will be accepted.
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));

  // Now attempt to show more dialogs.
  EXPECT_TRUE(manager().OnFormSubmitted(*CreateVehicleForm(kOtherLicensePlate),
                                        /*ukm_source_id=*/{}));
  // This one will be ignored because strikes were only reset for passports.
  EXPECT_FALSE(manager().OnFormSubmitted(*CreateVehicleForm(kOtherLicensePlate),
                                         /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(
      *CreatePassportForm(kOtherPassportNumber), /*ukm_source_id=*/{}));
}

// Tests that accepting a save prompt for an entity resets the strike counter
// for the strike key attributes of that entity.
TEST_F(AutofillAiManagerImportFormTest, AcceptingResetsStrikesPerAttribute) {
  {
    InSequence s;
    // First, we expect to see two save attempts for a passport.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    // We accept the next save prompt for a passport form.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // (User now deletes the passport.)

    // We now get more prompts for the same passport number again.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    PassportWithNumber(kDefaultPassportNumber), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
  }

  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  // This one will be accepted.
  ASSERT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));

  // User deletes the passport.
  ASSERT_THAT(GetEntityInstances(), SizeIs(1));
  RemoveEntityInstance(GetEntityInstances()[0].guid());

  EXPECT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
  EXPECT_TRUE(
      manager().OnFormSubmitted(*CreatePassportForm(), /*ukm_source_id=*/{}));
}

// Tests that migration prompts are only shown three times per url and entity
// type.
TEST_F(AutofillAiManagerImportFormTest, StrikesForMigrationPromptsPerUrl) {
  // A different vin number is also needed because it is part of the entity
  // strike keys. Otherwise the strike logic would stop showing the prompt but
  // not due to the url, rather the attributes.
  EntityInstance local_vehicle_entity =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner,
           .plate = kDefaultLicensePlate,
           .number = u"123"});
  AddOrUpdateEntityInstance(local_vehicle_entity);

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kOtherLicensePlate), _, _))
        .WillOnce(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kDeclineBubble));
  }

  std::unique_ptr<FormStructure> submitted_form_entity_with_default_plate =
      CreateVehicleForm();
  ASSERT_TRUE(manager().OnFormSubmitted(
      *submitted_form_entity_with_default_plate, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(
      *submitted_form_entity_with_default_plate, /*ukm_source_id=*/{}));
  // Note that this leads to a regular save prompt, however it is counted
  // towards the same strike database.
  std::unique_ptr<FormStructure> submitted_form_entity_with_other_plate =
      CreateVehicleForm(kOtherLicensePlate);
  ASSERT_TRUE(manager().OnFormSubmitted(*submitted_form_entity_with_other_plate,
                                        /*ukm_source_id=*/{}));

  // The fourth attempt is ignored.
  EXPECT_FALSE(manager().OnFormSubmitted(
      *submitted_form_entity_with_default_plate, /*ukm_source_id=*/{}));
  check.Call();

  // But submitting on a different page leads to a prompt.
  std::unique_ptr<FormStructure> other_domain_form =
      CreateVehicleForm(kDefaultLicensePlate, "https://other.com");
  EXPECT_TRUE(manager().OnFormSubmitted(*other_domain_form,
                                        /*ukm_source_id=*/{}));
}

// Tests that accepting a migration prompt for an entity resets the strike
// counter for that entity type.
TEST_F(AutofillAiManagerImportFormTest, AcceptingMigrationResetsStrikesPerUrl) {
  EntityInstance local_vehicle_entity_default_plate =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner, .plate = kDefaultLicensePlate});
  EntityInstance local_vehicle_entity_other_plate =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner, .plate = kOtherLicensePlate});
  AddOrUpdateEntityInstance(local_vehicle_entity_default_plate);
  AddOrUpdateEntityInstance(local_vehicle_entity_other_plate);
  MockFunction<void()> check;
  {
    InSequence s;
    // First, we expect to see two migration attempts for a vehicle.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    EXPECT_CALL(check, Call);

    // We accept the next migration prompt for a vehicle form.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // We now get two more vehicle migration prompts because vehicle strikes
    // were reset.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kOtherLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
  }

  std::unique_ptr<FormStructure> form = CreateVehicleForm();
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  check.Call();

  // This one will be accepted.
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // Now attempt to show more dialogs.
  std::unique_ptr<FormStructure> other_plate_form =
      CreateVehicleForm(kOtherLicensePlate);
  EXPECT_TRUE(manager().OnFormSubmitted(*other_plate_form,
                                        /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(*other_plate_form,
                                        /*ukm_source_id=*/{}));
}

// Tests that accepting a migration prompt for an entity resets the strike
// counter for the strike key attributes of that entity.
TEST_F(AutofillAiManagerImportFormTest,
       AcceptingMigrationResetsStrikesPerAttribute) {
  EntityInstance local_vehicle_entity_default_plate =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.name = kDefaultVehicleOwner, .plate = kDefaultLicensePlate});
  AddOrUpdateEntityInstance(local_vehicle_entity_default_plate);
  {
    InSequence s;
    // First, we expect to see two migration attempts for a vehicle.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
    // We accept the next migration prompt for a vehicle form.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .WillOnce(RunOnceCallback<2>(kAcceptBubble));

    // We now get more prompts for the same vehicle license plate again.
    EXPECT_CALL(autofill_client(),
                ShowEntityImportBubble(
                    VehicleWithLicensePlate(kDefaultLicensePlate), _, _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<2>(kDeclineBubble));
  }

  std::unique_ptr<FormStructure> form = CreateVehicleForm();
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // This one will be accepted.
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

// Test that disabling the property for, say, identity-related entities
// suppresses import.
TEST_F(AutofillAiManagerImportFormTest,
       DoNotImportIfPrefForEntityGroupDisabled) {
  using enum AttributeTypeName;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({NAME_FULL, DRIVERS_LICENSE_NUMBER});
  EntityInstance entity = test::GetDriversLicenseEntityInstance();
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("pref disabled"));
    EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
    EXPECT_CALL(check, Call("pref enabled"));
    EXPECT_CALL(autofill_client(), ShowEntityImportBubble);
  }

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiIdentityEntitiesEnabled, false);
  check.Call("pref disabled");
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiIdentityEntitiesEnabled, true);
  check.Call("pref enabled");
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerImportFormTest,
       EntityContainsRequiredAttributes_ShowPromptAndAccept) {
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));
  // Save prompts lead to a hats survey being triggered.
  EXPECT_CALL(autofill_client(),
              TriggerAutofillAiSavePromptSurvey(
                  /*prompt_accepted=*/true,
                  /*entity_type*/
                  EntityType(EntityTypeName::kPassport),
                  /*saved_entities=*/base::flat_set<EntityTypeName>({})));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  // This is a save bubble, `old_entity` should not exist.
  EXPECT_FALSE(old_entity.has_value());
  // Passport entities should always be local.
  EXPECT_EQ(new_entity->record_type(), EntityInstance::RecordType::kLocal);

  // Accept the bubble.
  std::move(save_callback).Run(kAcceptBubble);
  // Tests that the expected entity was saved.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 1u);
  const EntityInstance& saved_entity = *saved_entities.begin();
  EXPECT_EQ(saved_entity, *new_entity);
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entity, AttributeTypeName::kPassportName, /*app_locale=*/""),
      u"Jon Doe");
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entity, AttributeTypeName::kPassportNumber, /*app_locale=*/""),
      u"1234321");
}

TEST_F(AutofillAiManagerImportFormTest,
       EntityContainsRequiredAttributes_ShowPromptAndDecline) {
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(MoveArg<2>(&save_callback));
  // Save prompts lead to a hats survey being triggered.
  EXPECT_CALL(
      autofill_client(),
      TriggerAutofillAiSavePromptSurvey(
          /*prompt_accepted=*/false, EntityType(EntityTypeName::kPassport),
          /*saved_entities=*/base::flat_set<EntityTypeName>({})));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // Decline the bubble.
  std::move(save_callback).Run(kDeclineBubble);
  // Tests that the no entity was saved.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 0u);
}

TEST_F(AutofillAiManagerImportFormTest,
       EntityDoesNotContainRequiredAttributes_DoNotShowPrompt) {
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({PASSPORT_ISSUING_COUNTRY, NAME_FULL});
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(u"Germany");
  form->field(1)->set_value(u"1234321");

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // Tests that no entity was saved.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 0u);
}

// In this test, we simulate the user submitting a form with data that is
// already contained in one of the entities.
TEST_F(AutofillAiManagerImportFormTest, EntityAlreadyStored_DoNotShowPrompt) {
  using enum AttributeTypeName;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({NAME_FULL, DRIVERS_LICENSE_NUMBER});
  EntityInstance entity = test::GetDriversLicenseEntityInstance();
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(
      GetValueFromEntity(entity, AttributeType(kDriversLicenseName)));
  form->field(1)->set_value(
      GetValueFromEntity(entity, AttributeType(kDriversLicenseNumber)));
  AddOrUpdateEntityInstance(entity);

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // Tests that no entity was saved.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 1u);
}

TEST_F(AutofillAiManagerImportFormTest, NewEntity_ShowPromptAndAccept) {
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER});
  EntityInstance existing_entity = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(existing_entity);
  // Set the filled values to be different to the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<EntityInstance> entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));

  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  // This is a save bubble, `old_entity` should not exist.
  EXPECT_FALSE(old_entity.has_value());

  // Accept the bubble.
  std::move(save_callback).Run(kAcceptBubble);
  // Tests that the expected entity was saved.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 2u);
  EntityInstance saved_entity = *saved_entities.begin();
  if (saved_entity == existing_entity) {
    saved_entity = *(saved_entities.begin() + 1);
  }
  EXPECT_EQ(saved_entity, *entity);
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entity, AttributeTypeName::kPassportName, /*app_locale=*/""),
      u"Jon Doe");
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entity, AttributeTypeName::kPassportNumber, /*app_locale=*/""),
      u"1234321");
}

// If the new entity to be saved is a walletable entity type, it should lead to
// an entity that is stored in the server.
TEST_F(AutofillAiManagerImportFormTest,
       WalletableEntity_Save_RecordType_Server) {
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({NAME_FULL, VEHICLE_VIN});
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  // This is a save bubble, `old_entity` should not exist.
  EXPECT_FALSE(old_entity.has_value());
  EXPECT_EQ(new_entity->record_type(),
            EntityInstance::RecordType::kServerWallet);
}

// This test ensures that no save prompt is shown for an entity type
// that has no import constraints.
TEST_F(AutofillAiManagerImportFormTest,
       NewEntity_DoNotShowSavePromptWhenEntityTypeHasNoImportConstraints) {
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
       FLIGHT_RESERVATION_CONFIRMATION_CODE});
  form->field(0)->set_value(u"1234321");
  form->field(1)->set_value(u"567890");
  form->field(2)->set_value(u"ABC1234");

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

// This test ensures that no update prompt is shown for an entity type
// that has no import constraints.
TEST_F(AutofillAiManagerImportFormTest,
       NewEntity_DoNotShowUpdatePromptWhenEntityTypeHasNoImportConstraints) {
  test::FlightReservationOptions options{
      .flight_number = u"123456",
      .ticket_number = u"567890",
      .confirmation_code = u"ABC1234",
  };
  EntityInstance entity = test::GetFlightReservationEntityInstance(options);
  AddOrUpdateEntityInstance(entity);
  std::unique_ptr<FormStructure> form = CreateFormStructure(
      {FLIGHT_RESERVATION_FLIGHT_NUMBER, FLIGHT_RESERVATION_TICKET_NUMBER,
       FLIGHT_RESERVATION_CONFIRMATION_CODE});
  form->field(0)->set_value(options.flight_number);
  form->field(1)->set_value(options.ticket_number);
  form->field(2)->set_value(u"differentCode");

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerImportFormTest, UpdateEntity_NewInfo) {
  using enum AttributeTypeName;
  // The submitted form will have expiration date info.
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE});

  // The current entity however does not.
  EntityInstance existing_entity_without_expiry_dates =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  AddOrUpdateEntityInstance(existing_entity_without_expiry_dates);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, and also fill the expiry date.
  form->field(0)->set_value(GetValueFromEntity(
      existing_entity_without_expiry_dates, AttributeType(kPassportNumber)));
  form->field(1)->set_value(u"01/02/20");
  form->field(1)->set_format_string_unless_overruled(
      AutofillFormatString(u"DD/MM/YY", FormatString_Type_DATE),
      AutofillFormatStringSource::kServer);

  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));

  // An update bubble should be shown.
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(old_entity.has_value());
  // Passport entities are stored locally.
  ASSERT_EQ(new_entity->record_type(), EntityInstance::RecordType::kLocal);
  // Accept the bubble.
  std::move(save_callback).Run(kAcceptBubble);

  // Only one entity should exist, as it was updated.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 1u);
  const EntityInstance& saved_entity = saved_entities.front();

  EXPECT_EQ(*old_entity, existing_entity_without_expiry_dates);
  EXPECT_EQ(*new_entity, saved_entity);
  EXPECT_EQ(saved_entity.guid(), old_entity->guid());
  // The new expiry date information should be added accordingly.
  EXPECT_EQ(GetValueFromEntityForAttributeTypeName(
                saved_entity, AttributeTypeName::kPassportExpirationDate,
                /*app_locale=*/""),
            u"2020-02-01");
}

// If the entity to be updated is a walletable entity type, it should lead to an
// entity that is stored in the server, even if the original entity is stored
// locally.
TEST_F(AutofillAiManagerImportFormTest,
       WalletableEntity_Update_RecordType_Server) {
  using enum AttributeTypeName;
  // The submitted form will have license plate info.
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({VEHICLE_VIN, VEHICLE_LICENSE_PLATE});

  // The current entity however does not.
  EntityInstance existing_entity_without_license_plate =
      test::GetVehicleEntityInstance({.plate = nullptr});
  AddOrUpdateEntityInstance(existing_entity_without_license_plate);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, and also fill the expiry date.
  form->field(0)->set_value(GetValueFromEntity(
      existing_entity_without_license_plate, AttributeType(kVehicleVin)));
  form->field(1)->set_value(u"12345");

  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));

  // An update bubble should be shown.
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(old_entity.has_value());
  ASSERT_EQ(existing_entity_without_license_plate, *old_entity);
  ASSERT_EQ(old_entity->record_type(), EntityInstance::RecordType::kLocal);
  EXPECT_EQ(new_entity->record_type(),
            EntityInstance::RecordType::kServerWallet);
  // Accept the bubble.
  std::move(save_callback).Run(kAcceptBubble);
  EXPECT_THAT(GetEntityInstances(), testing::UnorderedElementsAre(new_entity));
}

// If the original entity is stored in the server but the updated entity is a
// local entity, no update prompt should happen. This could happen if the user
// has an original server entity but then opts out of the feature. In this
// scenario only local entities are created, however update prompts should not
// move a server entity to a local entity.
TEST_F(AutofillAiManagerImportFormTest,
       WalletableEntity_Update_RecordType_ServerToLocal_DoNotOfferUpdate) {
  using enum AttributeTypeName;
  // The submitted form will have license plate info.
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({VEHICLE_VIN, VEHICLE_LICENSE_PLATE});

  // The current entity however does not.
  EntityInstance existing_entity_without_license_plate =
      test::GetVehicleEntityInstance(
          {.plate = nullptr,
           .record_type = EntityInstance::RecordType::kServerWallet});
  AddOrUpdateEntityInstance(existing_entity_without_license_plate);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, and also fill the expiry date.
  form->field(0)->set_value(GetValueFromEntity(
      existing_entity_without_license_plate, AttributeType(kVehicleVin)));
  form->field(1)->set_value(u"12345");

  // Disable the wallet feature.
  autofill_client().GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, false);

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  ASSERT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerImportFormTest, UpdateEntity_UpdateInfo) {
  using enum AttributeTypeName;
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE});
  EntityInstance existing_entity =
      test::GetPassportEntityInstance({.expiry_date = u"2019-01-02"});
  AddOrUpdateEntityInstance(existing_entity);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, and also fill the expiry date.
  form->field(0)->set_value(
      GetValueFromEntity(existing_entity, AttributeType(kPassportNumber)));
  // Set a new expiry date value, different from the one currently stored.
  form->field(1)->set_value(u"01/02/20");
  form->field(1)->set_format_string_unless_overruled(
      AutofillFormatString(u"DD/MM/YY", FormatString_Type_DATE),
      AutofillFormatStringSource::kServer);

  std::optional<EntityInstance> new_entity;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback save_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));

  // An update bubble should be shown.
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_TRUE(old_entity.has_value());
  // Accept the bubble.
  std::move(save_callback).Run(kAcceptBubble);

  // Only one entity should exist, as it was updated.
  base::span<const EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 1u);
  const EntityInstance& saved_entity = saved_entities.front();

  EXPECT_EQ(*old_entity, existing_entity);
  EXPECT_EQ(*new_entity, saved_entity);
  EXPECT_EQ(saved_entity.guid(), old_entity->guid());
  // The expiry date information should be updated accordingly.
  EXPECT_EQ(GetValueFromEntityForAttributeTypeName(
                saved_entity, AttributeTypeName::kPassportExpirationDate,
                /*app_locale=*/""),
            u"2020-02-01");
}

TEST_F(AutofillAiManagerImportFormTest,
       UpdateEntity_DoNotShowPromptWhenEntityInstanceIsReadOnly) {
  // The submitted form will have issue date info.
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({NAME_FULL, PASSPORT_NUMBER, PASSPORT_ISSUE_DATE});

  // The current entity however does not, but is read only.
  EntityInstance existing_entity_without_issue_date =
      test::GetPassportEntityInstance({
          .issue_date = nullptr,
          .are_attributes_read_only =
              EntityInstance::AreAttributesReadOnly(true),
      });
  AddOrUpdateEntityInstance(existing_entity_without_issue_date);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, also fill the issue and expiry dates.
  form->field(0)->set_value(
      GetValueFromEntity(existing_entity_without_issue_date,
                         AttributeType(AttributeTypeName::kPassportName)));
  form->field(1)->set_value(
      GetValueFromEntity(existing_entity_without_issue_date,
                         AttributeType(AttributeTypeName::kPassportNumber)));
  // Issue date.
  form->field(2)->set_value(u"01/02/16");
  form->field(2)->set_format_string_unless_overruled(
      AutofillFormatString(u"DD/MM/YY", FormatString_Type_DATE),
      AutofillFormatStringSource::kServer);

  // Since the current entity instance is read only, no prompt should be shown.
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerImportFormTest, PromptSuppressionMetric) {
  std::unique_ptr<FormStructure> form =
      CreateFormStructure({DRIVERS_LICENSE_NUMBER, VEHICLE_VIN, VEHICLE_YEAR});

  EntityInstance vehicle =
      test::GetVehicleEntityInstance({.number = u"987654", .year = u""});
  // Clear vehicle year information so that we can simulate an update prompt
  // later at submission time.
  AddOrUpdateEntityInstance(vehicle);

  // This should create a save prompt for the driver's license.
  form->field(0)->set_value(u"123456");

  // this should create an update prompt for the vehicle.
  form->field(1)->set_value(
      vehicle.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(u"2025");

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  // Expect that the save prompt for the driver's license was shown and
  // consequently suppressed the update prompt for the vehicle.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.PromptSuppression.SavePrompt.DriversLicense", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.PromptSuppression.UpdatePrompt.Vehicle", 1, 1);
}

class AutofillAiManagerUpstreamTest : public AutofillAiManagerTest {
 public:
  AutofillAiManagerUpstreamTest() = default;

  std::unique_ptr<FormStructure> CreateTestForm() {
    auto form_structure = std::make_unique<FormStructure>(test::GetFormData({
        .fields = {{.role = NAME_FULL},
                   {.role = VEHICLE_VIN},
                   {.role = VEHICLE_LICENSE_PLATE}},
    }));
    test_api(*form_structure)
        .SetFieldTypes({NAME_FULL, VEHICLE_VIN, VEHICLE_LICENSE_PLATE});
    return form_structure;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWalletVehicleRegistration};
};

// Tests that a migration prompt is shown if the user enter data in the form
// that is a subset of some local entity.
TEST_F(AutofillAiManagerUpstreamTest, LocalEntity_ShowsMigrationPrompt) {
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance local_entity = test::GetVehicleEntityInstance();
  AddOrUpdateEntityInstance(local_entity);

  form->field(0)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      local_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble);
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

TEST_F(AutofillAiManagerUpstreamTest,
       TestGetEntityUpstreamCandidateTimingMetric) {
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance local_entity = test::GetVehicleEntityInstance();
  AddOrUpdateEntityInstance(local_entity);

  form->field(0)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      local_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.Timing.GetEntityUpstreamCandidateFromSubmittedForm", 1);
}

// Tests that a migration prompt is not shown for flight reservations.
TEST_F(AutofillAiManagerUpstreamTest,
       LocalFlightReservationEntity_DoNotShowMigrationPrompt) {
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance local_entity = test::GetFlightReservationEntityInstance();
  AddOrUpdateEntityInstance(local_entity);

  form->field(0)->set_value(
      local_entity
          .attribute(
              AttributeType(AttributeTypeName::kFlightReservationPassengerName))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity
          .attribute(
              AttributeType(AttributeTypeName::kFlightReservationTicketNumber))
          ->GetRawInfo(FLIGHT_RESERVATION_TICKET_NUMBER));
  form->field(2)->set_value(
      local_entity
          .attribute(
              AttributeType(AttributeTypeName::kFlightReservationFlightNumber))
          ->GetRawInfo(FLIGHT_RESERVATION_FLIGHT_NUMBER));

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble);
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

// This tests check that if more than one migration candidate exists, the one
// that was most recently used is chosen.
TEST_F(AutofillAiManagerUpstreamTest,
       MultipleCandidates_ShowsMigrationPromptForRecentlyUsedEntity) {
  base::Time january_2017 = base::Time::FromSecondsSinceUnixEpoch(1484505871);
  base::Time june_2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);
  EntityInstance local_entity_1 =
      test::GetVehicleEntityInstanceWithRandomGuid({.use_date = january_2017});
  EntityInstance local_entity_2 =
      test::GetVehicleEntityInstanceWithRandomGuid({.use_date = june_2017});
  AddOrUpdateEntityInstance(local_entity_1);
  AddOrUpdateEntityInstance(local_entity_2);

  std::unique_ptr<FormStructure> form = CreateTestForm();
  form->field(0)->set_value(
      local_entity_1.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity_1.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      local_entity_1
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  std::optional<EntityInstance> entity_to_upstream;
  std::optional<EntityInstance> old_entity;
  AutofillClient::EntityImportPromptResultCallback upstream_callback;
  EXPECT_CALL(autofill_client(), ShowEntityImportBubble)
      .WillOnce(DoAll(SaveArg<0>(&entity_to_upstream), SaveArg<1>(&old_entity),
                      MoveArg<2>(&upstream_callback)));
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
  ASSERT_FALSE(old_entity);
  ASSERT_TRUE(entity_to_upstream);
  // `local_entity_2` was recently used.
  EXPECT_EQ(entity_to_upstream->guid(), local_entity_2.guid());

  // Accept the bubble.
  std::move(upstream_callback).Run(kAcceptBubble);
  EXPECT_THAT(GetEntityInstances(), testing::UnorderedElementsAre(
                                        local_entity_1, entity_to_upstream));
}

TEST_F(AutofillAiManagerUpstreamTest, FeatureOff_DoNotShowMigrationPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAiWalletVehicleRegistration);
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance local_entity = test::GetVehicleEntityInstance();
  AddOrUpdateEntityInstance(local_entity);

  form->field(0)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      local_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

// Tests that disabling the pref for, say, travel-related entities suppresses
// the wallet migration prompt.
TEST_F(AutofillAiManagerUpstreamTest,
       PrefForEntityGroupOff_DoNotShowMigrationPrompt) {
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance local_entity = test::GetVehicleEntityInstance();
  AddOrUpdateEntityInstance(local_entity);

  form->field(0)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      local_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      local_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("pref disabled"));
    EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
    EXPECT_CALL(check, Call("pref enabled"));
    EXPECT_CALL(autofill_client(), ShowEntityImportBubble);
  }

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiTravelEntitiesEnabled, false);
  check.Call("pref disabled");
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiTravelEntitiesEnabled, true);
  check.Call("pref enabled");
  EXPECT_TRUE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

// Tests that a migration prompt is not shown if the user entered data in the
// form that is a subset of a server entity.
TEST_F(AutofillAiManagerUpstreamTest, ServerEntity_DoNotShowMigrationPrompt) {
  std::unique_ptr<FormStructure> form = CreateTestForm();
  EntityInstance server_entity = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  AddOrUpdateEntityInstance(server_entity);

  form->field(0)->set_value(
      server_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetRawInfo(NAME_FULL));
  form->field(1)->set_value(
      server_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetRawInfo(VEHICLE_VIN));
  form->field(2)->set_value(
      server_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetRawInfo(VEHICLE_LICENSE_PLATE));

  EXPECT_CALL(autofill_client(), ShowEntityImportBubble).Times(0);
  EXPECT_FALSE(manager().OnFormSubmitted(*form, /*ukm_source_id=*/{}));
}

}  // namespace
}  // namespace autofill
