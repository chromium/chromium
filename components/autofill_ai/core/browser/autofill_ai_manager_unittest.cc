// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/browser/webdata/entities/entity_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager_test_api.h"
#include "components/autofill_ai/core/browser/mock_autofill_ai_client.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/389629573): Refactor this test to handle only the
// implementation under `autofill::features::kAutofillAiWithDataSchema` flag.
namespace autofill_ai {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;
using enum SuggestionType;
using AutofillAiPayload = Suggestion::AutofillAiPayload;
using PredictionsByGlobalId = AutofillAiModelExecutor::PredictionsByGlobalId;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::VariantWith;

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

class MockAutofillAiModelExecutor : public AutofillAiModelExecutor {
 public:
  MOCK_METHOD(
      void,
      GetPredictions,
      (autofill::FormData form_data,
       (base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map),
       (base::flat_map<autofill::FieldGlobalId, bool> sensitivity_map),
       optimization_guide::proto::AXTreeUpdate ax_tree_update,
       PredictionsReceivedCallback callback),
      (override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsRequest>&,
      GetLatestRequest,
      (),
      (const override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsResponse>&,
      GetLatestResponse,
      (),
      (const override));
};

class BaseAutofillAiManagerTest : public testing::Test {
 public:
  BaseAutofillAiManagerTest() {
    ON_CALL(client(), GetAutofillClient)
        .WillByDefault(ReturnRef(autofill_client_));
    ON_CALL(client(), IsAutofillAiEnabledPref).WillByDefault(Return(true));
    ON_CALL(client(), IsUserEligible).WillByDefault(Return(true));
  }

  MockAutofillAiModelExecutor& model_executor() { return model_executor_; }
  MockAutofillAiClient& client() { return client_; }
  AutofillAiManager& manager() { return manager_; }
  autofill::TestStrikeDatabase& strike_database() { return strike_database_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  autofill::TestAutofillClient autofill_client_;
  NiceMock<MockAutofillAiModelExecutor> model_executor_;
  NiceMock<MockAutofillAiClient> client_;
  autofill::TestStrikeDatabase strike_database_;
  AutofillAiManager manager_{&client(), &strike_database_};
};

class AutofillAiManagerTest : public BaseAutofillAiManagerTest {
 public:
  AutofillAiManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillAi, {{"skip_allowlist", "true"},
                      {"extract_ax_tree_for_predictions", "true"},
                      {"send_title_url", "false"}});
    ON_CALL(client(), GetModelExecutor)
        .WillByDefault(Return(&model_executor()));
    ON_CALL(client(), GetEntityDataManager)
        .WillByDefault(Return(&entity_data_manager_));
    ON_CALL(client(), IsUserEligible).WillByDefault(Return(true));
  }

  // Given a `FormStructure` sets `field_types_predictions` for each field in
  // the form.
  void AddPredictionsToFormStructure(
      autofill::FormStructure& form_structure,
      const std::vector<std::vector<autofill::FieldType>>&
          field_types_predictions) {
    CHECK_EQ(form_structure.field_count(), field_types_predictions.size());
    for (size_t i = 0; i < form_structure.field_count(); i++) {
      std::vector<autofill::AutofillQueryResponse::FormSuggestion::
                      FieldSuggestion::FieldPrediction>
          predictions_for_field;
      for (autofill::FieldType type : field_types_predictions[i]) {
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction prediction;
        prediction.set_type(type);
        predictions_for_field.push_back(prediction);
      }

      form_structure.field(i)->set_server_predictions(predictions_for_field);
    }
  }

  void AddOrUpdateEntityInstance(autofill::EntityInstance entity) {
    entity_data_manager_.AddOrUpdateEntityInstance(std::move(entity));
    webdata_helper_.WaitUntilIdle();
  }

  std::vector<autofill::EntityInstance> GetEntityInstances() {
    webdata_helper_.WaitUntilIdle();
    return entity_data_manager_.GetEntityInstances();
  }

 private:
  autofill::AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<autofill::EntityTable>()};
  autofill::EntityDataManager entity_data_manager_{
      webdata_helper_.autofill_webdata_service()};
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillAiManagerTest, RejectedPromptStrikeCounting) {
  autofill::FormStructure form1{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(1));

  autofill::FormStructure form2{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(2));

  // Neither of the forms should be blocked in the beginning.
  EXPECT_FALSE(manager().IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));

  // After up to two strikes the form should not blocked.
  manager().AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager().IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));

  manager().AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager().IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));

  // After the third strike form1 should become blocked but form2 remains
  // unblocked.
  manager().AddStrikeForImportFromForm(form1);
  EXPECT_TRUE(manager().IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));

  // Now the second form received three strikes and gets eventually blocked.
  manager().AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));
  manager().AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));
  manager().AddStrikeForImportFromForm(form2);
  EXPECT_TRUE(manager().IsFormBlockedForImport(form2));

  // After resetting form2, form1 should remain blocked.
  manager().RemoveStrikesForImportFromForm(form2);
  EXPECT_TRUE(manager().IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager().IsFormBlockedForImport(form2));
}

// Tests that the user receives a filling suggestion when using AutofillAi
// manual fallback on a field that was previously classified as such. Since the
// field is already classified, no model call is required.
TEST_F(AutofillAiManagerTest,
       GetSuggestionsTriggeringFieldIsAutofillAi_ReturnFillingSuggestion) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure = autofill::FormStructure(form);
  AddPredictionsToFormStructure(
      form_structure, {{autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG}});
  ON_CALL(client(), GetCachedFormStructure)
      .WillByDefault(Return(&form_structure));

  AddOrUpdateEntityInstance(autofill::test::GetPassportEntityInstance());

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestions(form.global_id(), form.fields().front().global_id(),
                           /*is_manual_fallback=*/false,
                           suggestions.GetCallback());
  EXPECT_THAT(suggestions.Take(),
              ElementsAre(HasType(kFillAutofillAi), HasType(kSeparator),
                          HasType(kManageAutofillAi)));
}

// Tests that the user receives a loading suggestions when using AutofillAi
// manual fallback on a field that was not previously classified as such. This
// leads to a model call to classify the form.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestionsManualFallback_TriggeringFieldNotAutofillAi_ReturnLoadingSuggestion) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure = autofill::FormStructure(form);
  ON_CALL(client(), GetCachedFormStructure)
      .WillByDefault(Return(&form_structure));

  AddOrUpdateEntityInstance(autofill::test::GetPassportEntityInstance());
  base::MockCallback<AutofillAiManager::GetSuggestionsCallback>
      get_suggestions_callback;

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestions(form.global_id(), form.fields().front().global_id(),
                           /*is_manual_fallback=*/true,
                           suggestions.GetCallback());
  EXPECT_THAT(suggestions.Take(),
              ElementsAre(HasType(kAutofillAiLoadingState)));
}

// Tests that the user receives a filling suggestion when using AutofillAi
// manual fallback on a field that was previously classified as such. Since the
// field is already classified, no model call is required.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestionsManualFallback_TriggeringFieldIsAutofillAi_ReturnFillingSuggestion) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure = autofill::FormStructure(form);
  AddPredictionsToFormStructure(
      form_structure, {{autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG}});
  ON_CALL(client(), GetCachedFormStructure)
      .WillByDefault(Return(&form_structure));

  AddOrUpdateEntityInstance(autofill::test::GetPassportEntityInstance());
  base::MockCallback<AutofillAiManager::GetSuggestionsCallback>
      get_suggestions_callback;

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestions(form.global_id(), form.fields().front().global_id(),
                           /*is_manual_fallback=*/true,
                           suggestions.GetCallback());
  EXPECT_THAT(suggestions.Take(),
              ElementsAre(HasType(kFillAutofillAi), HasType(kSeparator),
                          HasType(kManageAutofillAi)));
}

class AutofillAiManagerImportFormTest
    : public AutofillAiManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AutofillAiManagerImportFormTest() = default;

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

  std::u16string GetValueFromEntityForFieldType(
      const autofill::EntityInstance entity,
      autofill::FieldType type) {
    std::optional<autofill::AttributeType> attribute =
        autofill::AttributeType::FromFieldType(type);
    CHECK(attribute);
    base::optional_ref<const autofill::AttributeInstance> instance =
        entity.attribute(*attribute);
    CHECK(instance);
    return instance->value();
  }

  std::u16string GetValueFromEntityForAttributeTypeName(
      const autofill::EntityInstance entity,
      autofill::AttributeTypeName type) {
    base::optional_ref<const autofill::AttributeInstance> instance =
        entity.attribute(autofill::AttributeType(type));
    CHECK(instance);
    return instance->value();
  }
};

TEST_F(AutofillAiManagerImportFormTest,
       EntityContainsRequiredAttributes_ShowPromptAndAccept) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_NAME_TAG, autofill::PASSPORT_NUMBER,
       autofill::PHONE_HOME_WHOLE_NUMBER});
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<autofill::EntityInstance> new_entity;
  std::optional<autofill::EntityInstance> old_entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(DoAll(SaveArg<0>(&new_entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());

  // This is a save bubble, `old_entity` should not exist.
  EXPECT_FALSE(old_entity.has_value());

  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  std::move(save_callback)
      .Run(AutofillAiClient::SavePromptAcceptanceResult(
          /*did_user_interact=*/true, new_entity));
  // Tests that the expected entity was saved.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 1u);
  EXPECT_EQ(saved_entities[0], *new_entity);
  EXPECT_EQ(GetValueFromEntityForAttributeTypeName(
                saved_entities[0], autofill::AttributeTypeName::kPassportName),
            u"Jon Doe");
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entities[0], autofill::AttributeTypeName::kPassportNumber),
      u"1234321");
}

TEST_F(AutofillAiManagerImportFormTest,
       EntityContainsRequiredAttributes_ShowPromptAndDecline) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_NAME_TAG, autofill::PASSPORT_NUMBER,
       autofill::PHONE_HOME_WHOLE_NUMBER});
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(MoveArg<2>(&save_callback));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Decline the bubble.
  std::move(save_callback).Run(AutofillAiClient::SavePromptAcceptanceResult());
  // Tests that the no entity was saved.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 0u);
}

TEST_F(AutofillAiManagerImportFormTest,
       EntityDoesNotContainRequiredAttributes_DoNotShowPrompt) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_ISSUING_COUNTRY_TAG, autofill::PASSPORT_NAME_TAG});
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(u"Germany");
  form->field(1)->set_value(u"1234321");

  EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // The prompt is not shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_FALSE(autofill_ai_shows_bubble);

  // Tests that no entity was saved.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 0u);
}

// In this test, we simulate the user submitting a form with data that is
// already contained in one of the entities.
TEST_F(AutofillAiManagerImportFormTest, EntityAlreadyStored_DoNotShowPrompt) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::LOYALTY_MEMBERSHIP_ID, autofill::LOYALTY_MEMBERSHIP_PROGRAM});
  autofill::EntityInstance entity =
      autofill::test::GetLoyaltyCardEntityInstance();
  // Set the filled values to be the same as the ones already stored.
  form->field(0)->set_value(
      GetValueFromEntityForFieldType(entity, autofill::LOYALTY_MEMBERSHIP_ID));
  form->field(1)->set_value(GetValueFromEntityForFieldType(
      entity, autofill::LOYALTY_MEMBERSHIP_PROGRAM));
  AddOrUpdateEntityInstance(entity);

  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // The prompt is not shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_FALSE(autofill_ai_shows_bubble);

  // Tests that no entity was saved.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();
  EXPECT_EQ(saved_entities.size(), 1u);
}

TEST_F(AutofillAiManagerImportFormTest, NewEntity_ShowPromptAndAccept) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_NAME_TAG, autofill::PASSPORT_NUMBER,
       autofill::PHONE_HOME_WHOLE_NUMBER});
  autofill::EntityInstance existing_entity =
      autofill::test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(existing_entity);
  // Set the filled values to be different to the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<autofill::EntityInstance> entity;
  std::optional<autofill::EntityInstance> old_entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(DoAll(SaveArg<0>(&entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());

  // This is a save bubble, `old_entity` should not exist.
  EXPECT_FALSE(old_entity.has_value());

  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  std::move(save_callback)
      .Run(AutofillAiClient::SavePromptAcceptanceResult(
          /*did_user_interact=*/true, entity));
  // Tests that the expected entity was saved.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();
  ASSERT_EQ(saved_entities.size(), 2u);
  EXPECT_EQ(saved_entities[1], *entity);
  EXPECT_EQ(GetValueFromEntityForAttributeTypeName(
                saved_entities[1], autofill::AttributeTypeName::kPassportName),
            u"Jon Doe");
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entities[1], autofill::AttributeTypeName::kPassportNumber),
      u"1234321");
}

TEST_F(AutofillAiManagerImportFormTest, UpdateEntity_ShowPromptAndAccept) {
  // The submitted form will have issue date info.
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_NAME_TAG, autofill::PASSPORT_NUMBER,
       autofill::PASSPORT_ISSUE_DATE_TAG,
       autofill::PASSPORT_EXPIRATION_DATE_TAG});

  // The current entity however does not.
  autofill::test::PassportEntityOptions
      passport_without_issue_date_and_expiry_date;
  passport_without_issue_date_and_expiry_date.issue_date = nullptr;
  passport_without_issue_date_and_expiry_date.expiry_date = u"";
  autofill::EntityInstance existing_entity_without_issue_and_expiry_dates =
      autofill::test::GetPassportEntityInstance(
          passport_without_issue_date_and_expiry_date);
  AddOrUpdateEntityInstance(existing_entity_without_issue_and_expiry_dates);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, also fill the issue and expiry dates.
  form->field(0)->set_value(GetValueFromEntityForFieldType(
      existing_entity_without_issue_and_expiry_dates,
      autofill::PASSPORT_NAME_TAG));
  form->field(1)->set_value(GetValueFromEntityForFieldType(
      existing_entity_without_issue_and_expiry_dates,
      autofill::PASSPORT_NUMBER));
  // Issue date
  form->field(2)->set_value(u"01/02/2016");
  // Expirty date
  form->field(3)->set_value(u"01/02/2020");

  std::optional<autofill::EntityInstance> entity;
  std::optional<autofill::EntityInstance> old_entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(DoAll(SaveArg<0>(&entity), SaveArg<1>(&old_entity),
                      MoveArg<2>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());

  // This is an update bubble, `old_entity` should exist.
  EXPECT_TRUE(old_entity.has_value());
  EXPECT_EQ(*old_entity, existing_entity_without_issue_and_expiry_dates);

  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  std::move(save_callback)
      .Run(AutofillAiClient::SavePromptAcceptanceResult(
          /*did_user_interact=*/true, entity));
  // Tests that the expected entity was updated.
  std::vector<autofill::EntityInstance> saved_entities = GetEntityInstances();

  // Only one entity should exist, as it was updated.
  ASSERT_EQ(saved_entities.size(), 1u);
  EXPECT_EQ(saved_entities[0], *entity);
  EXPECT_EQ(saved_entities[0].guid(),
            existing_entity_without_issue_and_expiry_dates.guid());
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entities[0], autofill::AttributeTypeName::kPassportIssueDate),
      u"01/02/2016");
  EXPECT_EQ(
      GetValueFromEntityForAttributeTypeName(
          saved_entities[0], autofill::AttributeTypeName::kPassportExpiryDate),
      u"01/02/2020");
}

// Tests that `import_form_callback` is run with an empty list of entries when
// `user_annotations::ShouldAddFormSubmissionForURL()` returns `false`.
TEST_F(AutofillAiManagerTest,
       MaybeImportFormRunsCallbackWithFalseWhenImportIsNotAttempted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"allowed_hosts_for_form_submissions", "otherhost.com"}});
  auto ineligible_form_structure =
      std::make_unique<autofill::FormStructure>(autofill::FormData());

  base::MockOnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                              bool autofill_ai_shows_bubble)>
      autofill_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
  EXPECT_CALL(autofill_callback,
              Run(Pointer(ineligible_form_structure.get()), false));
  manager().MaybeImportForm(std::move(ineligible_form_structure),
                            autofill_callback.Get());
}

class IsFormAndFieldEligibleAutofillAiTest : public BaseAutofillAiManagerTest {
 public:
  IsFormAndFieldEligibleAutofillAiTest() {
    autofill::test::FormDescription form_description = {
        .fields = {{.role = autofill::NAME_FIRST,
                    .heuristic_type = autofill::NAME_FIRST}}};
  }

  void SetPredictionTypesForField(autofill::AutofillField& field,
                                  autofill::FieldTypeSet types) {
    std::vector<autofill::AutofillQueryResponse::FormSuggestion::
                    FieldSuggestion::FieldPrediction>
        predictions;
    for (autofill::FieldType type : types) {
      autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
          FieldPrediction prediction;
      prediction.set_type(type);
      predictions.push_back(prediction);
    }

    field.set_server_predictions(std::move(predictions));
  }

  std::unique_ptr<autofill::FormStructure> CreateEligibleForm(
      const GURL& url = GURL("https://example.com")) {
    autofill::FormData form_data;
    form_data.set_main_frame_origin(url::Origin::Create(url));
    auto form = std::make_unique<autofill::FormStructure>(form_data);
    autofill::AutofillField& field = test_api(*form).PushField();
    SetPredictionTypesForField(
        field, {autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG});
    return form;
  }
};

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsNotEligibleIfBothFlagsAreDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disable_features*/ {
          kAutofillAi, autofill::features::kAutofillAiWithDataSchema});
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  EXPECT_FALSE(manager().IsEligibleForAutofillAi(*form, *form->field(0)));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsNotEligibleIfServerPredictionHasNoAutofillAiType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  SetPredictionTypesForField(*form->field(0), {autofill::NAME_FIRST});

  EXPECT_FALSE(manager().IsEligibleForAutofillAi(*form, *form->field(0)));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleIfPrefIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  EXPECT_CALL(client(), IsAutofillAiEnabledPref).WillOnce(Return(false));
  EXPECT_FALSE(manager().IsEligibleForAutofillAi(*form, *form->field(0)));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, AutofillAiEligibility_Eligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAiWithDataSchema},
      /*disable_features*/ {kAutofillAi});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  EXPECT_TRUE(manager().IsEligibleForAutofillAi(*form, *form->field(0)));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleForNonEligibleUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  ON_CALL(client(), IsUserEligible).WillByDefault(Return(false));
  EXPECT_FALSE(manager().IsEligibleForAutofillAi(*form, *form->field(0)));
}

}  // namespace
}  // namespace autofill_ai
