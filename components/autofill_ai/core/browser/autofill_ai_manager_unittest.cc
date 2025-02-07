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
#include "components/autofill/core/browser/data_manager/entities/test_entity_data_manager.h"
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
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
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

const GURL& url() {
  static GURL url = GURL("https://example.com");
  return url;
}

const url::Origin& origin() {
  static url::Origin origin = url::Origin::Create(url());
  return origin;
}

class BaseAutofillAiManagerTest : public testing::Test {
 public:
  BaseAutofillAiManagerTest() {
    ON_CALL(client(), GetAutofillClient)
        .WillByDefault(ReturnRef(autofill_client_));
    ON_CALL(client(), IsAutofillAiEnabledPref).WillByDefault(Return(true));
    ON_CALL(client(), IsUserEligible).WillByDefault(Return(true));
  }

  optimization_guide::MockOptimizationGuideDecider& decider() {
    return decider_;
  }
  MockAutofillAiModelExecutor& model_executor() { return model_executor_; }
  MockAutofillAiClient& client() { return client_; }
  AutofillAiManager& manager() { return manager_; }
  autofill::TestStrikeDatabase& strike_database() { return strike_database_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  autofill::TestAutofillClient autofill_client_;
  NiceMock<optimization_guide::MockOptimizationGuideDecider> decider_;
  NiceMock<MockAutofillAiModelExecutor> model_executor_;
  NiceMock<MockAutofillAiClient> client_;
  autofill::TestStrikeDatabase strike_database_;
  AutofillAiManager manager_{&client(), &decider(), &strike_database_};
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
    ON_CALL(client(), GetLastCommittedOrigin)
        .WillByDefault(ReturnRef(origin()));
    ON_CALL(client(), GetTitle).WillByDefault(Return("title"));
    ON_CALL(client(), GetUserAnnotationsService)
        .WillByDefault(Return(&user_annotations_service_));
    ON_CALL(client(), GetEntityDataManager)
        .WillByDefault(Return(&test_entity_data_manager_));
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

 protected:
  user_annotations::TestUserAnnotationsService user_annotations_service_;
  autofill::TestEntityDataManager test_entity_data_manager_;
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

// Tests that when the server fails to return suggestions, we show an error
// suggestion.
TEST_F(AutofillAiManagerTest, RetrievalFailed_ShowError) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;

  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kAutofillAiLoadingState)), _));
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate()));
    EXPECT_CALL(model_executor(), GetPredictions)
        .WillOnce(RunOnceCallback<4>(PredictionsByGlobalId{}, ""));
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kAutofillAiError), HasType(kSeparator),
                                HasType(kAutofillAiFeedback)),
                    _));
  }

  manager().OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  base::test::RunUntil([this]() {
    return !test_api(manager()).loading_suggestion_timer().IsRunning();
  });
}

// Tests that when the server fails to generate suggestions, but we have
// autofill suggestions stored already, we fallback to autofill and don't show
// error suggestions.
TEST_F(AutofillAiManagerTest, RetrievalFailed_FallbackToAutofill) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  std::vector<Suggestion> autofill_suggestions = {Suggestion(kAddressEntry),
                                                  Suggestion(kSeparator),
                                                  Suggestion(kManageAddress)};
  test_api(manager()).SetAutofillSuggestions(autofill_suggestions);

  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;
  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kAutofillAiLoadingState)), _));
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate()));
    EXPECT_CALL(model_executor(), GetPredictions)
        .WillOnce(RunOnceCallback<4>(PredictionsByGlobalId{}, ""));
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kAddressEntry), HasType(kSeparator),
                                HasType(kManageAddress)),
                    _));
  }

  manager().OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  base::test::RunUntil([this]() {
    return !test_api(manager()).loading_suggestion_timer().IsRunning();
  });
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

  test_entity_data_manager_.AddEntityInstance(
      autofill::test::GetPassportEntityInstance());

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestionsV2(
      form.global_id(), form.fields().front().global_id(),
      /*is_manual_fallback=*/false, suggestions.GetCallback());
  EXPECT_THAT(suggestions.Take(), ElementsAre(HasType(kFillAutofillAi)));
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

  test_entity_data_manager_.AddEntityInstance(
      autofill::test::GetPassportEntityInstance());
  base::MockCallback<AutofillAiManager::GetSuggestionsCallback>
      get_suggestions_callback;

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestionsV2(
      form.global_id(), form.fields().front().global_id(),
      /*is_manual_fallback=*/true, suggestions.GetCallback());
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

  test_entity_data_manager_.AddEntityInstance(
      autofill::test::GetPassportEntityInstance());
  base::MockCallback<AutofillAiManager::GetSuggestionsCallback>
      get_suggestions_callback;

  base::test::TestFuture<std::vector<autofill::Suggestion>> suggestions;
  manager().GetSuggestionsV2(
      form.global_id(), form.fields().front().global_id(),
      /*is_manual_fallback=*/true, suggestions.GetCallback());
  EXPECT_THAT(suggestions.Take(), ElementsAre(HasType(kFillAutofillAi)));
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
    return base::UTF8ToUTF16(instance->value());
  }

  std::u16string GetValueFromEntityForAttributeTypeName(
      const autofill::EntityInstance entity,
      autofill::AttributeTypeName type) {
    base::optional_ref<const autofill::AttributeInstance> instance =
        entity.attribute(autofill::AttributeType(type));
    CHECK(instance);
    return base::UTF8ToUTF16(instance->value());
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

  std::optional<autofill::EntityInstance> entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(
          testing::DoAll(SaveArg<0>(&entity), MoveArg<1>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  AutofillAiClient::SavePromptAcceptanceResult callback_result =
      AutofillAiClient::SavePromptAcceptanceResult(
          /*prompt_was_accepted=*/true);
  callback_result.entity = entity;
  std::move(save_callback).Run(callback_result);
  // Tests that the expected entity was saved.
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();
  EXPECT_EQ(saved_entities.size(), 1u);
  EXPECT_EQ(saved_entities[0], *entity);
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
      .WillOnce(MoveArg<1>(&save_callback));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  std::move(save_callback)
      .Run(AutofillAiClient::SavePromptAcceptanceResult(
          /*prompt_was_accepted=*/false));
  // Tests that the no entity was saved.
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();
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
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();
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
  test_entity_data_manager_.AddEntityInstance(entity);

  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // The prompt is not shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_FALSE(autofill_ai_shows_bubble);

  // Tests that no entity was saved.
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();
  EXPECT_EQ(saved_entities.size(), 1u);
}

TEST_F(AutofillAiManagerImportFormTest, NewEntity_ShowPromptAndAccept) {
  std::unique_ptr<autofill::FormStructure> form = CreateFormStructure(
      {autofill::PASSPORT_NAME_TAG, autofill::PASSPORT_NUMBER,
       autofill::PHONE_HOME_WHOLE_NUMBER});
  autofill::EntityInstance existing_entity =
      autofill::test::GetPassportEntityInstance();
  test_entity_data_manager_.AddEntityInstance(existing_entity);
  // Set the filled values to be different to the ones already stored.
  form->field(0)->set_value(u"Jon Doe");
  form->field(1)->set_value(u"1234321");

  std::optional<autofill::EntityInstance> entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(
          testing::DoAll(SaveArg<0>(&entity), MoveArg<1>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  AutofillAiClient::SavePromptAcceptanceResult callback_result =
      AutofillAiClient::SavePromptAcceptanceResult(
          /*prompt_was_accepted=*/true);
  callback_result.entity = entity;
  std::move(save_callback).Run(callback_result);
  // Tests that the expected entity was saved.
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();
  EXPECT_EQ(saved_entities.size(), 2u);
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
  passport_without_issue_date_and_expiry_date.expiry_date = "";
  autofill::EntityInstance existing_entity_without_issue_date =
      autofill::test::GetPassportEntityInstance(
          passport_without_issue_date_and_expiry_date);
  test_entity_data_manager_.AddEntityInstance(
      existing_entity_without_issue_date);

  // Set the filled values to be the same as the ones already stored in the
  // existing entity, also fill the issue and expiry dates.
  form->field(0)->set_value(GetValueFromEntityForFieldType(
      existing_entity_without_issue_date, autofill::PASSPORT_NAME_TAG));
  form->field(1)->set_value(GetValueFromEntityForFieldType(
      existing_entity_without_issue_date, autofill::PASSPORT_NUMBER));
  // Issue date
  form->field(2)->set_value(u"01/02/2016");
  // Expirty date
  form->field(3)->set_value(u"01/02/2020");

  std::optional<autofill::EntityInstance> entity;
  AutofillAiClient::SavePromptAcceptanceCallback save_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble)
      .WillOnce(
          testing::DoAll(SaveArg<0>(&entity), MoveArg<1>(&save_callback)));
  base::test::TestFuture<std::unique_ptr<autofill::FormStructure>, bool>
      autofill_callback;
  manager().MaybeImportForm(std::move(form), autofill_callback.GetCallback());
  // Tell the caller the bubble was shown.
  const bool autofill_ai_shows_bubble = std::get<1>(autofill_callback.Take());
  EXPECT_TRUE(autofill_ai_shows_bubble);

  // Accept the bubble.
  AutofillAiClient::SavePromptAcceptanceResult callback_result =
      AutofillAiClient::SavePromptAcceptanceResult(
          /*prompt_was_accepted=*/true);
  callback_result.entity = entity;
  std::move(save_callback).Run(callback_result);
  // Tests that the expected entity was updated.
  base::test::TestFuture<std::vector<autofill::EntityInstance>>
      saved_instances_callback;
  test_entity_data_manager_.LoadEntityInstances(
      saved_instances_callback.GetCallback());
  std::vector<autofill::EntityInstance> saved_entities =
      saved_instances_callback.Take();

  // Only one entity should exist, as it was updated.
  EXPECT_EQ(saved_entities.size(), 1u);
  EXPECT_EQ(saved_entities[0], *entity);
  EXPECT_EQ(saved_entities[0].guid(),
            existing_entity_without_issue_date.guid());
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
              Run(Pointer(ineligible_form_structure.get()), false))
      .Times(1);
  manager().MaybeImportForm(std::move(ineligible_form_structure),
                            autofill_callback.Get());
}

// Tests that the prediction improvements settings page is opened when the
// manage prediction improvements link is clicked.
TEST_F(AutofillAiManagerTest, OpenSettingsWhenManagePILinkIsClicked) {
  EXPECT_CALL(client(), OpenAutofillAiSettings);
  manager().UserClickedLearnMore();
}

// Tests that calling `OnLoadingSuggestionShown()` is a no-op if the
// `kTriggerAutomatically` parameter is disabled.
TEST_F(AutofillAiManagerTest,
       OnLoadingSuggestionShownDoesNothingIfParamNotEnabled) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;
  EXPECT_CALL(update_suggestions_callback, Run).Times(0);
  EXPECT_CALL(client(), GetAXTree).Times(0);
  manager().OnSuggestionsShown({kAutofillAiLoadingState}, form,
                               form.fields().front(),
                               update_suggestions_callback.Get());
}

// TODO(crbug.com/376016081): Refactor test to expect if suggestions are
// included so that `ShouldSkipAutofillSuggestion()` can be move to the
// anonymous namespace.
TEST_F(AutofillAiManagerTest, ShouldSkipAutofillSuggestion) {
  Suggestion autofill_suggestion = Suggestion(kAddressEntry);
  autofill_suggestion.payload =
      Suggestion::AutofillProfilePayload(Suggestion::Guid("guid"));
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::FieldType::NAME_FIRST},
                 {.role = autofill::FieldType::NAME_LAST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure{form};
  test_api(form_structure)
      .SetFieldTypes(
          {autofill::FieldType::NAME_FIRST, autofill::FieldType::NAME_LAST});
  ON_CALL(client(), GetCachedFormStructure)
      .WillByDefault(Return(&form_structure));
  EXPECT_CALL(client(), GetAutofillNameFillingValue(
                            _, autofill::FieldType::NAME_FIRST, _))
      .WillOnce(Return(u"j ǎ Ņ ë"));
  EXPECT_CALL(client(),
              GetAutofillNameFillingValue(_, autofill::FieldType::NAME_LAST, _))
      .WillOnce(Return(u"  d o Ê"));
  const PredictionsByGlobalId cache = PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {u"Jane", u"First Name", form.fields()[0].IsFocusable()}},
      {form.fields()[1].global_id(),
       {u"Doe", u"Last Name", form.fields()[1].IsFocusable()}}};
  EXPECT_TRUE(
      ShouldSkipAutofillSuggestion(client(), cache, form, autofill_suggestion));
}

class AutofillAiManagerTriggerAutomaticallyTest
    : public BaseAutofillAiManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillAiManagerTriggerAutomaticallyTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillAi,
        {{"skip_allowlist", "true"},
         {"trigger_automatically", "true"},
         {"extract_ax_tree_for_predictions", GetParam() ? "true" : "false"}});
    ON_CALL(client(), GetLastCommittedOrigin)
        .WillByDefault(ReturnRef(origin()));
    ON_CALL(client(), GetModelExecutor)
        .WillByDefault(Return(&model_executor()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that calling `OnLoadingSuggestionShown()` results in retrieving the AX
// tree (implying predictions will be attempted to be retrieved) if the
// `kTriggerAutomatically` parameter is enabled.
TEST_P(AutofillAiManagerTriggerAutomaticallyTest,
       OnLoadingSuggestionShownGetsAXTreeIfParamEnabled) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;
  if (GetParam()) {
    EXPECT_CALL(client(), GetAXTree);
  }
  manager().OnSuggestionsShown({kAutofillAiLoadingState}, form,
                               form.fields().front(),
                               update_suggestions_callback.Get());
}

INSTANTIATE_TEST_SUITE_P(,
                         AutofillAiManagerTriggerAutomaticallyTest,
                         testing::Bool());

class IsFormAndFieldEligibleAutofillAiTest : public BaseAutofillAiManagerTest {
 public:
  IsFormAndFieldEligibleAutofillAiTest() {
    ON_CALL(client(), GetLastCommittedOrigin)
        .WillByDefault(ReturnRef(origin()));
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
    autofill::AutofillField& prediction_improvement_field =
        test_api(*form).PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    prediction_improvement_field.set_heuristic_type(
        autofill::HeuristicSource::kAutofillAiRegexes,
        autofill::IMPROVED_PREDICTION);
#else
    prediction_improvement_field.set_heuristic_type(
        autofill::HeuristicSource::kLegacyRegexes,
        autofill::IMPROVED_PREDICTION);
#endif
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
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsNotEligibleIfServerPredictionHasNoAutofillAiType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);
  SetPredictionTypesForField(*prediction_improvement_field,
                             {autofill::NAME_FIRST});

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleIfPrefIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);
  SetPredictionTypesForField(
      *prediction_improvement_field,
      {autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG});

  EXPECT_CALL(client(), IsAutofillAiEnabledPref).WillOnce(Return(false));
  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsEligibleIfOptimizationGuideCanBeApplied) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);
  SetPredictionTypesForField(
      *prediction_improvement_field,
      {autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG});

  ON_CALL(decider(), CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  EXPECT_TRUE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, AutofillAiEligibility_Eligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAiWithDataSchema},
      /*disable_features*/ {kAutofillAi});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);
  SetPredictionTypesForField(
      *prediction_improvement_field,
      {autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG});

  EXPECT_TRUE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}
TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleForNonEligibleUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);
  SetPredictionTypesForField(
      *prediction_improvement_field,
      {autofill::NAME_FIRST, autofill::PASSPORT_NAME_TAG});

  ON_CALL(client(), IsUserEligible).WillByDefault(Return(false));
  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

}  // namespace
}  // namespace autofill_ai
