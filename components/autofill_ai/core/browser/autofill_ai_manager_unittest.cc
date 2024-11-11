// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

#include "base/task/current_thread.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
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

namespace autofill_ai {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;
using enum SuggestionType;
using PredictionImprovementsPayload = Suggestion::PredictionImprovementsPayload;
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
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
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

auto HasPredictionImprovementsPayload(auto expected_payload) {
  return Field("Suggestion::payload", &Suggestion::payload,
               VariantWith<PredictionImprovementsPayload>(expected_payload));
}

auto HasValueToFill(const std::u16string& expected_value_to_fill) {
  return Field("Suggestion::payload", &Suggestion::payload,
               VariantWith<Suggestion::ValueToFill>(
                   Suggestion::ValueToFill(expected_value_to_fill)));
}

auto HasMainText(const std::u16string& expected_main_text) {
  return Field("Suggestion::main_text", &Suggestion::main_text,
               Field("Suggestion::Text::value", &Suggestion::Text::value,
                     expected_main_text));
}

auto HasLabel(const std::u16string& expected_label) {
  return Field("Suggestion::labels", &Suggestion::labels,
               ElementsAre(ElementsAre(Field("Suggestion::Text::value",
                                             &Suggestion::Text::value,
                                             expected_label))));
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
    ON_CALL(client(), IsUserEligible).WillByDefault(Return(true));
  }

 protected:
  user_annotations::TestUserAnnotationsService user_annotations_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillAiManagerTest, RejctedPromptStrikeCounting) {
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
    EXPECT_CALL(
        update_suggestions_callback,
        Run(ElementsAre(HasType(kPredictionImprovementsLoadingState)), _));
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate()));
    EXPECT_CALL(model_executor(), GetPredictions)
        .WillOnce(RunOnceCallback<4>(PredictionsByGlobalId{}, ""));
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kPredictionImprovementsError),
                                HasType(kSeparator),
                                HasType(kPredictionImprovementsFeedback)),
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
    EXPECT_CALL(
        update_suggestions_callback,
        Run(ElementsAre(HasType(kPredictionImprovementsLoadingState)), _));
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

// Tests that the `update_suggestions_callback` is called eventually with the
// `kFillPredictionImprovements` suggestion.
TEST_F(AutofillAiManagerTest, EndToEnd) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  // Filled form, as returned by the model executor.
  form_description.host_frame = form.host_frame();
  form_description.renderer_id = form.renderer_id();
  form_description.fields[0].value = u"John";
  form_description.fields[0].host_frame = form.fields().front().host_frame();
  form_description.fields[0].renderer_id = form.fields().front().renderer_id();
  autofill::FormData filled_form =
      autofill::test::GetFormData(form_description);
  AutofillAiModelExecutor::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;

  const autofill::FormFieldData& filled_field = filled_form.fields().front();
  {
    InSequence s;
    EXPECT_CALL(
        update_suggestions_callback,
        Run(ElementsAre(HasType(kPredictionImprovementsLoadingState)), _));
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate()));
    EXPECT_CALL(model_executor(), GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(
        update_suggestions_callback,
        Run(AllOf(ElementsAre(HasType(kFillPredictionImprovements),
                              HasType(kSeparator),
                              HasType(kPredictionImprovementsFeedback)),
                  FirstElementIs(HasPredictionImprovementsPayload(
                      Field(&PredictionImprovementsPayload::values_to_fill,
                            ElementsAre(Pair(filled_field.global_id(),
                                             filled_field.value()))))),
                  FirstElementIs(Field(
                      &Suggestion::children,
                      ElementsAre(
                          HasType(kFillPredictionImprovements),
                          HasType(kSeparator),
                          HasType(kFillPredictionImprovements),
                          HasType(kSeparator),
                          HasType(kEditPredictionImprovementsInformation))))),
            _));
  }

  manager().OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());

  const std::vector<Suggestion> suggestions_while_loading =
      manager().GetSuggestions({}, filled_form, filled_form.fields().front());
  ASSERT_FALSE(suggestions_while_loading.empty());
  EXPECT_THAT(suggestions_while_loading[0],
              HasType(kPredictionImprovementsLoadingState));

  std::move(predictions_received_callback)
      .Run(PredictionsByGlobalId{{filled_field.global_id(),
                                  {filled_field.value(), filled_field.label(),
                                   filled_field.IsFocusable()}}},
           "");
  base::test::RunUntil([this]() {
    return !test_api(manager()).loading_suggestion_timer().IsRunning();
  });
}

// Tests that when the user triggers suggestions on a field having autofill
// suggestions, but then changes focus while predictions are loading to a field
// that doesn't have autofill suggestion, the initial autofill suggestions are
// cleared and not used.
TEST_F(AutofillAiManagerTest, AutofillSuggestionsAreCachedOnMultipleFocus) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::NAME_LAST,
                  .heuristic_type = autofill::NAME_LAST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);

  AutofillAiModelExecutor::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<AutofillAiManager::UpdateSuggestionsCallback>
      update_suggestions_callback;

  {
    InSequence s;
    EXPECT_CALL(
        update_suggestions_callback,
        Run(ElementsAre(HasType(kPredictionImprovementsLoadingState)), _));
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate()));
    EXPECT_CALL(model_executor(), GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback,
                Run(ElementsAre(HasType(kPredictionImprovementsError),
                                HasType(kSeparator),
                                HasType(kPredictionImprovementsFeedback)),
                    _));
  }

  std::vector<Suggestion> autofill_suggestions = {Suggestion(kAddressEntry),
                                                  Suggestion(kSeparator),
                                                  Suggestion(kManageAddress)};
  manager().GetSuggestions(autofill_suggestions, form, form.fields().front());
  manager().OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());

  // Simulate the user clicking on a second field AFTER triggering filling
  // suggestions but BEFORE the server replies with the predictions (hence in
  // the loading stage).
  manager().GetSuggestions({}, form, form.fields().back());

  // Simulate empty server response.
  std::move(predictions_received_callback).Run(PredictionsByGlobalId{}, "");
  base::test::RunUntil([this]() {
    return !test_api(manager()).loading_suggestion_timer().IsRunning();
  });
}

struct GetSuggestionsFormNotEqualCachedFormTestData {
  AutofillAiManager::PredictionRetrievalState prediction_retrieval_state;
  bool trigger_automatically;
  std::optional<SuggestionType> expected_suggestion_type;
};

class AutofillAiManagerGetSuggestionsFormNotEqualCachedFormTest
    : public BaseAutofillAiManagerTest,
      public ::testing::WithParamInterface<
          GetSuggestionsFormNotEqualCachedFormTestData> {
 public:
  AutofillAiManagerGetSuggestionsFormNotEqualCachedFormTest() {
    const GetSuggestionsFormNotEqualCachedFormTestData& test_data = GetParam();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillAi, {{"skip_allowlist", "true"},
                      {"trigger_automatically",
                       test_data.trigger_automatically ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that `GetSuggestions()` returns suggestions as expected when the
// requesting form doesn't match the cached form.
TEST_P(AutofillAiManagerGetSuggestionsFormNotEqualCachedFormTest,
       GetSuggestions_ReturnsSuggestionsAsExpected) {
  autofill::FormData cached_form =
      autofill::test::GetFormData(autofill::test::FormDescription{});
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  test_api(manager()).SetLastQueriedFormGlobalId(cached_form.global_id());
  if (GetParam().expected_suggestion_type) {
    EXPECT_THAT(manager().GetSuggestions({}, form, form.fields().front()),
                ElementsAre(HasType(*GetParam().expected_suggestion_type)));
  } else {
    EXPECT_THAT(manager().GetSuggestions({}, form, form.fields().front()),
                ElementsAre());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillAiManagerGetSuggestionsFormNotEqualCachedFormTest,
    testing::Values(
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state = AutofillAiManager::
                PredictionRetrievalState::kIsLoadingPredictions,
            .trigger_automatically = false,
            .expected_suggestion_type = std::nullopt},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = false,
            .expected_suggestion_type = kRetrievePredictionImprovements},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneError,
            .trigger_automatically = false,
            .expected_suggestion_type = kRetrievePredictionImprovements},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state = AutofillAiManager::
                PredictionRetrievalState::kIsLoadingPredictions,
            .trigger_automatically = true,
            .expected_suggestion_type = std::nullopt},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = true,
            .expected_suggestion_type = kPredictionImprovementsLoadingState},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneError,
            .trigger_automatically = true,
            .expected_suggestion_type = kPredictionImprovementsLoadingState}));

// Tests that trigger suggestions are returned by `GetSuggestions()` when the
// class is in `kReady` state.
TEST_F(AutofillAiManagerTest, GetSuggestions_Ready_ReturnsTriggerSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kReady);
  EXPECT_THAT(manager().GetSuggestions({}, form, field),
              ElementsAre(HasType(kRetrievePredictionImprovements)));
}

// Tests that loading suggestions are returned by `GetSuggestions()` when the
// class is in `kIsLoadingPredictions` state.
TEST_F(AutofillAiManagerTest,
       GetSuggestions_IsLoadingPredictions_ReturnsLoadingSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kIsLoadingPredictions);
  EXPECT_THAT(
      manager().GetSuggestions(/*autofill_suggestions=*/{}, form, field),
      ElementsAre(HasType(kPredictionImprovementsLoadingState)));
}

struct FallbackTestData {
  AutofillAiManager::PredictionRetrievalState prediction_retrieval_state;
  bool trigger_automatically;
};

class AutofillAiManagerDoneFallbackTest
    : public BaseAutofillAiManagerTest,
      public ::testing::WithParamInterface<FallbackTestData> {
 public:
  AutofillAiManagerDoneFallbackTest() {
    const FallbackTestData& test_data = GetParam();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillAi, {{"skip_allowlist", "true"},
                      {"trigger_automatically",
                       test_data.trigger_automatically ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that an empty vector is returned by `GetSuggestions()` when the
// class is in `kDoneSuccess` state, there are no prediction improvements for
// the `field` but there are `autofill_suggestions` to fall back to. Note that
// returning an empty vector would continue the regular Autofill flow in the
// BrowserAutofillManager, i.e. show Autofill suggestions in this scenario.
TEST_P(AutofillAiManagerDoneFallbackTest,
       GetSuggestions_NoPredictionsWithAutofillSuggestions_ReturnsEmptyVector) {
  std::vector<Suggestion> autofill_suggestions = {Suggestion(kAddressEntry)};
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  EXPECT_TRUE(
      manager().GetSuggestions(autofill_suggestions, form, field).empty());
}

// Tests that the no info / error suggestion is returned by `GetSuggestions()`
// when the class is in `kDoneSuccess` state, there are neither prediction
// improvements for the `field` nor `autofill_suggestions` to fall back to and
// the no info suggestion wasn't shown yet.
TEST_P(
    AutofillAiManagerDoneFallbackTest,
    GetSuggestions_NoPredictionsNoAutofillSuggestions_ReturnsNoInfoOrErrorSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  const std::vector<Suggestion> suggestions =
      manager().GetSuggestions(/*autofill_suggestions=*/{}, form, field);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0], HasType(kPredictionImprovementsError));
}

// Tests that the trigger suggestion is returned by `GetSuggestions()` when the
// class is in `kDoneSuccess` state, there are neither prediction improvements
// for the `field` nor `autofill_suggestions` to fall back to and the no info
// suggestion was shown before.
TEST_P(
    AutofillAiManagerDoneFallbackTest,
    GetSuggestions_NoPredictionsNoAutofillSuggestionsNoInfoWasShown_ReturnsTriggerSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  test_api(manager()).SetErrorOrNoInfoSuggestionShown(true);
  EXPECT_THAT(
      manager().GetSuggestions(/*autofill_suggestions=*/{}, form, field),
      ElementsAre(HasType(kRetrievePredictionImprovements)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillAiManagerDoneFallbackTest,
    testing::Values(
        FallbackTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = false},
        FallbackTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = true},
        FallbackTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneError,
            .trigger_automatically = false},
        FallbackTestData{
            .prediction_retrieval_state =
                AutofillAiManager::PredictionRetrievalState::kDoneError,
            .trigger_automatically = true}));

// Tests that cached filling suggestions for prediction improvements are shown
// before autofill suggestions.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestions_DoneSuccessWithAutofillSuggestions_PredictionImprovementsSuggestionsShownBeforeAutofill) {
  std::vector<Suggestion> autofill_suggestions = {Suggestion(kAddressEntry),
                                                  Suggestion(kSeparator),
                                                  Suggestion(kManageAddress)};
  autofill_suggestions[0].payload =
      Suggestion::AutofillProfilePayload(Suggestion::Guid("guid"));
  EXPECT_CALL(client(), GetAutofillNameFillingValue).WillOnce(Return(u""));
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure = autofill::FormStructure(form);
  test_api(form_structure).SetFieldTypes({autofill::FieldType::NAME_FIRST});
  EXPECT_CALL(client(), GetCachedFormStructure)
      .WillRepeatedly(Return(&form_structure));
  test_api(manager()).SetAutofillSuggestions(autofill_suggestions);
  test_api(manager()).SetCache(
      PredictionsByGlobalId{{form.fields().front().global_id(),
                             {u"value", u"label", /*is_focusable=*/true}}});
  test_api(manager()).SetLastQueriedFormGlobalId(form.global_id());
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  EXPECT_THAT(manager().GetSuggestions(autofill_suggestions, form,
                                       form.fields().front()),
              ElementsAre(HasType(kFillPredictionImprovements),
                          HasType(kAddressEntry), HasType(kSeparator),
                          HasType(kPredictionImprovementsFeedback)));
}

// Tests that the filling suggestion incl. its children is created as expected
// if state is `kDoneSuccess`.
TEST_F(AutofillAiManagerTest,
       GetSuggestions_DoneSuccess_ReturnsFillingSuggestions) {
  const std::u16string trigger_field_value = u"Jane";
  const std::u16string trigger_field_label = u"First name";
  const std::u16string select_field_value = u"33";
  const std::u16string select_field_label = u"State";
  const std::u16string select_field_option_text = u"North Carolina";
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::ADDRESS_HOME_STATE,
                  .heuristic_type = autofill::ADDRESS_HOME_STATE,
                  .form_control_type = autofill::FormControlType::kSelectOne},
                 // An existing prediction for a non-focusable field won't show
                 // up in child suggestions.
                 {.role = autofill::ADDRESS_HOME_CITY,
                  .heuristic_type = autofill::ADDRESS_HOME_CITY,
                  .is_focusable = false}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {trigger_field_value, trigger_field_label,
        form.fields()[0].IsFocusable()}},
      {form.fields()[1].global_id(),
       {select_field_value, select_field_label, form.fields()[1].IsFocusable(),
        select_field_option_text}},
      {form.fields()[2].global_id(),
       {u"value", u"label", form.fields()[2].IsFocusable()}}});
  test_api(manager()).SetLastQueriedFormGlobalId(form.global_id());
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  EXPECT_THAT(
      manager().GetSuggestions(/*autofill_suggestions=*/{}, form,
                               form.fields()[0]),
      ElementsAre(
          AllOf(
              HasType(kFillPredictionImprovements),
              HasPredictionImprovementsPayload(Field(
                  "PredictionImprovementsPayload::values_to_fill",
                  &PredictionImprovementsPayload::values_to_fill,
                  ElementsAre(
                      Pair(form.fields()[0].global_id(), trigger_field_value),
                      Pair(form.fields()[1].global_id(), select_field_value)))),
              Field("Suggestion::children", &Suggestion::children,
                    ElementsAre(
                        AllOf(HasType(kFillPredictionImprovements),
                              HasPredictionImprovementsPayload(_)),
                        HasType(kSeparator),
                        AllOf(HasType(kFillPredictionImprovements),
                              HasValueToFill(trigger_field_value),
                              HasMainText(trigger_field_value),
                              HasLabel(trigger_field_label)),
                        AllOf(HasType(kFillPredictionImprovements),
                              // For <select> elements expect both value to fill
                              // and main text to be set to the option text, not
                              // the value.
                              HasValueToFill(select_field_option_text),
                              HasMainText(select_field_option_text),
                              HasLabel(select_field_label)),
                        HasType(kSeparator),
                        HasType(kEditPredictionImprovementsInformation)))),
          HasType(kSeparator), HasType(kPredictionImprovementsFeedback)));
}

// Tests that the filling suggestion label is correct when only one field can be
// filled.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestions_DoneSuccessOneFieldCanBeFilled_CreateLabelThatContainsOnlyOneFieldData) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {u"Jane", u"First name", form.fields()[0].IsFocusable()}}});
  test_api(manager()).SetLastQueriedFormGlobalId(form.global_id());
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  const std::vector<Suggestion> suggestions = manager().GetSuggestions(
      /*autofill_suggestions=*/{}, form, form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0], HasLabel(u"Fill First name"));
}

// Tests that the filling suggestion label is correct when 3 fields can be
// filled.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestions_DoneSuccessThreeFieldsCanBeFilled_UserSingularAndMoreString) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = autofill::ADDRESS_HOME_STREET_NAME},
                 {.role = autofill::ADDRESS_HOME_STATE,
                  .heuristic_type = autofill::ADDRESS_HOME_STATE,
                  .form_control_type = autofill::FormControlType::kSelectOne}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {u"Jane", u"First name", form.fields()[0].IsFocusable()}},
      {form.fields()[1].global_id(),
       {u"Country roads str", u"Street name", form.fields()[1].IsFocusable()}},
      {form.fields()[2].global_id(),
       {u"33", u"state", form.fields()[2].IsFocusable(), u"West Virginia"}}});
  test_api(manager()).SetLastQueriedFormGlobalId(form.global_id());
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  const std::vector<Suggestion> suggestions = manager().GetSuggestions(
      /*autofill_suggestions=*/{}, form, form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasLabel(u"Fill First name, Street name & 1 more field"));
}

// Tests that the filling suggestion label is correct when more than 3 fields
// can be filled.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestions_DoneSuccessMoreThanThreeFieldsCanBeFilled_UserPluralAndMoreString) {
  autofill::test::FormDescription form_description = {
      .fields = {
          {.role = autofill::NAME_FIRST,
           .heuristic_type = autofill::NAME_FIRST},
          {.role = autofill::NAME_LAST, .heuristic_type = autofill::NAME_LAST},
          {.role = autofill::ADDRESS_HOME_STREET_NAME,
           .heuristic_type = autofill::ADDRESS_HOME_STREET_NAME},
          {.role = autofill::ADDRESS_HOME_STATE,
           .heuristic_type = autofill::ADDRESS_HOME_STATE,
           .form_control_type = autofill::FormControlType::kSelectOne}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {u"Jane", u"First name", form.fields()[0].IsFocusable()}},
      {form.fields()[1].global_id(),
       {u"Doe", u"Last name", form.fields()[1].IsFocusable()}},
      {form.fields()[2].global_id(),
       {u"Country roads str", u"Street name", form.fields()[2].IsFocusable()}},
      {form.fields()[3].global_id(),
       {u"33", u"state", form.fields()[3].IsFocusable(), u"West Virginia"}}});
  test_api(manager()).SetLastQueriedFormGlobalId(form.global_id());
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  const std::vector<Suggestion> suggestions =
      manager().GetSuggestions({}, form, form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasLabel(u"Fill First name, Last name & 2 more fields"));
}

// Tests that on field focus the potentially new state of the form fields'
// focusability is set in the cache.
TEST_F(AutofillAiManagerTest,
       GetSuggestions_kDoneSuccess_UpdatesFieldFocusabilityInCache) {
  // Set up manager to reflect having received predictions successfully for two
  // form fields, one of which is not focusable at the time of retrieval.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST, .is_focusable = false},
                 {.role = autofill::NAME_LAST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(manager()).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {u"Jane", u"First name", form.fields()[0].IsFocusable()}},
      {form.fields()[1].global_id(),
       {u"Doe", u"Last name", form.fields()[1].IsFocusable()}}});
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kDoneSuccess);

  // Now swap focusability of the two form fields.
  test_api(form).fields()[0].set_is_focusable(!form.fields()[0].IsFocusable());
  test_api(form).fields()[1].set_is_focusable(!form.fields()[1].IsFocusable());

  // With the above setup, `GetSuggestions()` is expected to call
  // `UpdateFieldFocusabilityInCache()`.
  manager().GetSuggestions(/*autofill_suggestions=*/{}, form, form.fields()[0]);

  EXPECT_THAT(test_api(manager()).GetCache(),
              Optional(ElementsAre(
                  Pair(form.fields()[0].global_id(),
                       Field("Prediction::is_focusable",
                             &AutofillAiModelExecutor::Prediction::is_focusable,
                             form.fields()[0].IsFocusable())),
                  Pair(form.fields()[1].global_id(),
                       Field("Prediction::is_focusable",
                             &AutofillAiModelExecutor::Prediction::is_focusable,
                             form.fields()[1].IsFocusable())))));
}

class AutofillAiManagerUserFeedbackTest
    : public AutofillAiManagerTest,
      public testing::WithParamInterface<AutofillAiManager::UserFeedback> {};

// Given a non-null feedback id, tests that an attempt to open the feedback page
// is only made if `UserFeedback::kThumbsDown` was received.
TEST_P(AutofillAiManagerUserFeedbackTest,
       TryToOpenFeedbackPageNeverCalledIfUserFeedbackThumbsDown) {
  using UserFeedback = AutofillAiManager::UserFeedback;
  test_api(manager()).SetFormFillingPredictionsModelExecutionId(
      "randomstringrjb");
  EXPECT_CALL(client(), TryToOpenFeedbackPage)
      .Times(GetParam() == UserFeedback::kThumbsDown);
  manager().UserFeedbackReceived(GetParam());
}

// Tests that the feedback page will never be opened if no feedback id is set.
TEST_P(AutofillAiManagerUserFeedbackTest,
       TryToOpenFeedbackPageNeverCalledIfNoFeedbackIdPresent) {
  test_api(manager()).SetFormFillingPredictionsModelExecutionId(std::nullopt);
  EXPECT_CALL(client(), TryToOpenFeedbackPage).Times(0);
  manager().UserFeedbackReceived(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillAiManagerUserFeedbackTest,
    testing::Values(AutofillAiManager::UserFeedback::kThumbsUp,
                    AutofillAiManager::UserFeedback::kThumbsDown));

class AutofillAiManagerImportFormTest
    : public AutofillAiManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AutofillAiManagerImportFormTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillAi, {{"should_extract_ax_tree_for_forms_annotations",
                       should_extract_ax_tree() ? "true" : "false"}});
  }

  bool should_import_form_data() const { return std::get<0>(GetParam()); }
  bool should_extract_ax_tree() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `import_form_callback` is run with added entries if the import was
// successful.
TEST_P(AutofillAiManagerImportFormTest,
       MaybeImportFormRunsCallbackWithAddedEntriesWhenImportWasSuccessful) {
  user_annotations_service_.AddHostToFormAnnotationsAllowlist(url().host());
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_data = autofill::test::GetFormData(form_description);
  std::unique_ptr<autofill::FormStructure> eligible_form_structure =
      std::make_unique<autofill::FormStructure>(form_data);

  test_api(*eligible_form_structure)
      .PushField()
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      .set_heuristic_type(
          autofill::HeuristicSource::kPredictionImprovementRegexes,
          autofill::IMPROVED_PREDICTION);
#else
      .set_heuristic_type(autofill::GetActiveHeuristicSource(),
                          autofill::IMPROVED_PREDICTION);
#endif
  if (should_extract_ax_tree()) {
    EXPECT_CALL(client(), GetAXTree)
        .WillOnce(
            RunOnceCallback<0>(optimization_guide::proto::AXTreeUpdate{}));
  } else {
    EXPECT_CALL(client(), GetAXTree).Times(0);
  }
  user_annotations_service_.SetShouldImportFormData(should_import_form_data());

  base::MockOnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                              bool autofill_ai_shows_bubble)>
      autofill_callback;
  if (should_import_form_data()) {
    EXPECT_CALL(client(),
                ShowSaveAutofillAiBubble(
                    Pointee(Field(&user_annotations::FormAnnotationResponse::
                                      to_be_upserted_entries,
                                  Not(IsEmpty()))),
                    _));
    EXPECT_CALL(autofill_callback,
                Run(Pointer(eligible_form_structure.get()), true));
  } else {
    EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
    EXPECT_CALL(autofill_callback,
                Run(Pointer(eligible_form_structure.get()), false));
  }
  manager().MaybeImportForm(std::move(eligible_form_structure),
                            autofill_callback.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillAiManagerImportFormTest,
    testing::Combine(/*should_import_form_data=*/testing::Bool(),
                     /*extract_ax_tree=*/testing::Bool()));

// Tests that if the pref is disabled, `import_form_callback` is run with an
// empty list of entries and nothing is forwarded to the
// `user_annotations_service_`.
TEST_F(AutofillAiManagerTest, FormNotImportedWhenPrefDisabled) {
  user_annotations_service_.AddHostToFormAnnotationsAllowlist(url().host());
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_data = autofill::test::GetFormData(form_description);
  std::unique_ptr<autofill::FormStructure> eligible_form_structure =
      std::make_unique<autofill::FormStructure>(form_data);

  test_api(*eligible_form_structure)
      .PushField()
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      .set_heuristic_type(
          autofill::HeuristicSource::kPredictionImprovementRegexes,
          autofill::IMPROVED_PREDICTION);
#else
      .set_heuristic_type(autofill::GetActiveHeuristicSource(),
                          autofill::IMPROVED_PREDICTION);
#endif
  user_annotations_service_.SetShouldImportFormData(
      /*should_import_form_data=*/true);

  base::MockOnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                              bool autofill_ai_shows_bubble)>
      autofill_callback;
  EXPECT_CALL(client(), ShowSaveAutofillAiBubble).Times(0);
  EXPECT_CALL(client(), GetAXTree).Times(0);
  EXPECT_CALL(client(), IsAutofillAiEnabledPref).WillOnce(Return(false));
  EXPECT_CALL(autofill_callback,
              Run(Pointer(eligible_form_structure.get()), false))
      .Times(1);
  manager().MaybeImportForm(std::move(eligible_form_structure),
                            autofill_callback.Get());
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

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(true)` if there's data stored in the user annotations.
TEST_F(AutofillAiManagerTest, HasDataStoredReturnsTrueIfDataIsStored) {
  base::MockCallback<AutofillAiManager::HasDataCallback> has_data_callback;
  user_annotations_service_.ReplaceAllEntries(
      {optimization_guide::proto::UserAnnotationsEntry()});
  manager().HasDataStored(has_data_callback.Get());
  EXPECT_CALL(has_data_callback, Run(AutofillAiManager::HasData(true)));
  manager().HasDataStored(has_data_callback.Get());
}

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(false)` if there's no data stored in the user annotations.
TEST_F(AutofillAiManagerTest, HasDataStoredReturnsFalseIfDataIsNotStored) {
  base::MockCallback<AutofillAiManager::HasDataCallback> has_data_callback;
  user_annotations_service_.ReplaceAllEntries({});
  manager().HasDataStored(has_data_callback.Get());
  EXPECT_CALL(has_data_callback, Run(AutofillAiManager::HasData(false)));
  manager().HasDataStored(has_data_callback.Get());
}

// Tests that the prediction improvements settings page is opened when the
// manage prediction improvements link is clicked.
TEST_F(AutofillAiManagerTest, OpenSettingsWhenManagePILinkIsClicked) {
  EXPECT_CALL(client(), OpenPredictionImprovementsSettings);
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
  manager().OnSuggestionsShown({kPredictionImprovementsLoadingState}, form,
                               form.fields().front(),
                               update_suggestions_callback.Get());
}

// Tests that the regular Autofill flow continues if predictions are being
// retrieved for form A, while a field of form B is focused.
TEST_F(AutofillAiManagerTest,
       GetSuggestionsReturnsEmptyVectorIfRequestedFromNewFormWhileLoading) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_a = autofill::test::GetFormData(form_description);
  manager().OnClickedTriggerSuggestion(form_a, form_a.fields().front(),
                                       base::DoNothing());

  autofill::FormData form_b = autofill::test::GetFormData(form_description);

  EXPECT_TRUE(manager()
                  .GetSuggestions(/*autofill_suggestions=*/{}, form_b,
                                  form_b.fields().front())
                  .empty());
}

// Tests that the trigger suggestion is shown if predictions were retrieved for
// form A and now a field of form B is focused.
TEST_F(
    AutofillAiManagerTest,
    GetSuggestionsReturnsTriggerSuggestionIfRequestedFromNewFormAndNotLoading) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_a = autofill::test::GetFormData(form_description);
  test_api(manager()).SetLastQueriedFormGlobalId(form_a.global_id());

  autofill::FormData form_b = autofill::test::GetFormData(form_description);

  const std::vector<Suggestion> suggestions = manager().GetSuggestions(
      /*autofill_suggestions=*/{}, form_b, form_b.fields().front());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0], HasType(kRetrievePredictionImprovements));
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
  EXPECT_CALL(client(), GetCachedFormStructure)
      .WillRepeatedly(Return(&form_structure));
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
  manager().OnSuggestionsShown({kPredictionImprovementsLoadingState}, form,
                               form.fields().front(),
                               update_suggestions_callback.Get());
}

INSTANTIATE_TEST_SUITE_P(,
                         AutofillAiManagerTriggerAutomaticallyTest,
                         testing::Bool());

// Tests that the loading suggestion is returned by `GetSuggestions()` when the
// class is in `kReady` state.
TEST_P(AutofillAiManagerTriggerAutomaticallyTest,
       GetSuggestions_Ready_ReturnsLoadingSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(manager()).SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState::kReady);
  EXPECT_THAT(manager().GetSuggestions({}, form, field),
              ElementsAre(HasType(kPredictionImprovementsLoadingState)));
}

class IsFormAndFieldEligibleAutofillAiTest : public BaseAutofillAiManagerTest {
 public:
  IsFormAndFieldEligibleAutofillAiTest() {
    ON_CALL(client(), GetLastCommittedOrigin)
        .WillByDefault(ReturnRef(origin()));
    autofill::test::FormDescription form_description = {
        .fields = {{.role = autofill::NAME_FIRST,
                    .heuristic_type = autofill::NAME_FIRST}}};
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
        autofill::HeuristicSource::kPredictionImprovementRegexes,
        autofill::IMPROVED_PREDICTION);
#else
    prediction_improvement_field.set_heuristic_type(
        autofill::HeuristicSource::kLegacyRegexes,
        autofill::IMPROVED_PREDICTION);
#endif
    return form;
  }
};

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleIfFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAutofillAi);
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleIfDeciderIsNull) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});
  AutofillAiManager manager{&client(), nullptr, &strike_database()};
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager.IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsEligibleIfSkipAllowlistIsTrue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_TRUE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleIfPrefIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});

  EXPECT_CALL(client(), IsAutofillAiEnabledPref).WillOnce(Return(false));

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsNotEligibleIfOptimizationGuideCannotBeApplied) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "false"}});
  ON_CALL(decider(), CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       IsEligibleIfOptimizationGuideCanBeApplied) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "false"}});
  ON_CALL(decider(), CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_TRUE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleForNotHttps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "false"}});

  std::unique_ptr<autofill::FormStructure> form =
      CreateEligibleForm(GURL("http://http.com"));
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleOnEmptyForm) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});

  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::AutofillField field;

  EXPECT_FALSE(manager().IsEligibleForAutofillAi(form, field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest,
       PredictionImprovementsEligibility_Eligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_TRUE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillAiTest, IsNotEligibleForNonEligibleUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillAi, {{"skip_allowlist", "true"}});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  ON_CALL(client(), IsUserEligible).WillByDefault(Return(false));
  EXPECT_FALSE(
      manager().IsEligibleForAutofillAi(*form, *prediction_improvement_field));
}

}  // namespace
}  // namespace autofill_ai
