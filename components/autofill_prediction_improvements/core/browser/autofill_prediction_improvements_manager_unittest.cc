// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

class MockAutofillPredictionImprovementsClient
    : public AutofillPredictionImprovementsClient {
 public:
  MOCK_METHOD(void,
              GetAXTree,
              (AutofillPredictionImprovementsClient::AXTreeCallback callback),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsManager&,
              GetManager,
              (),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsFillingEngine*,
              GetFillingEngine,
              (),
              (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (override));
};

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

class MockAutofillPredictionImprovementsFillingEngine
    : public AutofillPredictionImprovementsFillingEngine {
 public:
  MOCK_METHOD(void,
              GetPredictions,
              (autofill::FormData form_data,
               optimization_guide::proto::AXTreeUpdate ax_tree_update,
               PredictionsReceivedCallback callback),
              (override));
};

class BaseAutofillPredictionImprovementsManagerTest : public testing::Test {
 protected:
  GURL url_{"https://example.com"};
  NiceMock<MockOptimizationGuideDecider> decider_;
  NiceMock<MockAutofillPredictionImprovementsFillingEngine> filling_engine_;
  NiceMock<MockAutofillPredictionImprovementsClient> client_;
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;
  base::test::ScopedFeatureList feature_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
};

class AutofillPredictionImprovementsManagerTest
    : public BaseAutofillPredictionImprovementsManagerTest {
 public:
  AutofillPredictionImprovementsManagerTest() {
    feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                                {{"skip_allowlist", "true"}});
    ON_CALL(client_, GetFillingEngine).WillByDefault(Return(&filling_engine_));
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_);
  }

 protected:
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;
};

// Tests that the `update_suggestions_callback` is called eventually with the
// `kFillPredictionImprovements` suggestion.
TEST_F(AutofillPredictionImprovementsManagerTest, EndToEnd) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  // Filled form, as returned by the filling engine.
  form_description.fields[0].value = u"John";
  form_description.fields[0].host_frame = form.fields().front().host_frame();
  form_description.fields[0].renderer_id = form.fields().front().renderer_id();
  autofill::FormData filled_form =
      autofill::test::GetFormData(form_description);
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<autofill::Suggestion> loading_suggestion;
  std::vector<autofill::Suggestion> filling_suggestion;

  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
    EXPECT_CALL(filling_engine_, GetPredictions)
        .WillOnce(MoveArg<2>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&filling_suggestion));
  }

  manager_->OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  std::move(axtree_received_callback).Run({});
  std::move(predictions_received_callback).Run(filled_form);

  EXPECT_THAT(
      loading_suggestion,
      ElementsAre(Field(
          &autofill::Suggestion::type,
          Eq(autofill::SuggestionType::kPredictionImprovementsLoadingState))));
  ASSERT_THAT(filling_suggestion,
              ElementsAre(Field(
                  &autofill::Suggestion::type,
                  Eq(autofill::SuggestionType::kFillPredictionImprovements))));
  autofill::Suggestion::PredictionImprovementsPayload filling_payload =
      filling_suggestion[0]
          .GetPayload<autofill::Suggestion::PredictionImprovementsPayload>();
  const autofill::FormFieldData& filled_field = filled_form.fields().front();
  EXPECT_THAT(
      filling_payload.values_to_fill,
      ElementsAre(Pair(filled_field.global_id(), filled_field.value())));
}

// Tests that no suggestions are added to `address_suggestions` if
// `should_add_trigger_suggestion` is `false`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsDoesNotUpdateIfItShouldNot) {
  std::vector<autofill::Suggestion> address_suggestions;
  autofill::FormFieldData field;
  EXPECT_FALSE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/false));
}

// Tests that `address_suggestions` only contains the
// `kRetrievePredictionImprovements` suggestion if it was empty before calling
// `MaybeUpdateSuggestions()`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsOnEmptyAddressSuggestionsAddsTriggerSuggestion) {
  std::vector<autofill::Suggestion> address_suggestions;
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(Field(
          &autofill::Suggestion::type,
          Eq(autofill::SuggestionType::kRetrievePredictionImprovements))));
}

// Tests that `address_suggestions` contains the
// `kRetrievePredictionImprovements` suggestion (incl. a separator) if
// `address_suggestions` contained entries before calling
// `MaybeUpdateSuggestions()`. The pre-existing entries contained a
// `kUndoOrClear` suggestion which is the case for autofilled fields.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    MaybeUpdateSuggestionsOnAddressSuggestionsIncludingUndoOrClearAddsTriggerSuggestion) {
  std::vector<autofill::Suggestion> address_suggestions = {
      autofill::Suggestion(autofill::SuggestionType::kAddressEntry),
      autofill::Suggestion(autofill::SuggestionType::kSeparator),
      autofill::Suggestion(autofill::SuggestionType::kUndoOrClear),
      autofill::Suggestion(autofill::SuggestionType::kManageAddress)};
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kAddressEntry)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kSeparator)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kRetrievePredictionImprovements)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kSeparator)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kUndoOrClear)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kManageAddress))));
}

// Tests that `address_suggestions` contains the
// `kRetrievePredictionImprovements` suggestion (incl. a separator) if
// `address_suggestions` contained entries before calling
// `MaybeUpdateSuggestions()`. The pre-existing entries did not contain a
// `kUndoOrClear` suggestion which is the case for fields that aren't
// autofilled.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsOnAddressSuggestionsAddsTriggerSuggestion) {
  std::vector<autofill::Suggestion> address_suggestions = {
      autofill::Suggestion(autofill::SuggestionType::kAddressEntry),
      autofill::Suggestion(autofill::SuggestionType::kSeparator),
      autofill::Suggestion(autofill::SuggestionType::kManageAddress)};
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kAddressEntry)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kSeparator)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kRetrievePredictionImprovements)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kSeparator)),
          Field(&autofill::Suggestion::type,
                Eq(autofill::SuggestionType::kManageAddress))));
}

class ShouldProvideAutofillPredictionImprovementsTest
    : public BaseAutofillPredictionImprovementsManagerTest {
 public:
  ShouldProvideAutofillPredictionImprovementsTest() {
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
    autofill::test::FormDescription form_description = {
        .fields = {{.role = autofill::NAME_FIRST,
                    .heuristic_type = autofill::NAME_FIRST}}};
    form_ = autofill::test::GetFormData(form_description);
  }

 protected:
  autofill::FormData form_;
};

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfFlagDisabled) {
  feature_.InitAndDisableFeature(kAutofillPredictionImprovements);
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager.OnClickedTriggerSuggestion(
      form_, form_.fields().front(),
      /*update_suggestions_callback=*/base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfDeciderIsNull) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, nullptr};
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager.OnClickedTriggerSuggestion(
      form_, form_.fields().front(),
      /*update_suggestions_callback=*/base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       ExtractsImprovedPredictionsIfSkipAllowlistIsTrue) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  EXPECT_CALL(client_, GetAXTree);
  manager.OnClickedTriggerSuggestion(
      form_, form_.fields().front(),
      /*update_suggestions_callback=*/base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfOptimizationGuideCannotBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager.OnClickedTriggerSuggestion(
      form_, form_.fields().front(),
      /*update_suggestions_callback=*/base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       ExtractsImprovedPredictionsIfOptimizationGuideCanBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  EXPECT_CALL(client_, GetAXTree);
  manager.OnClickedTriggerSuggestion(
      form_, form_.fields().front(),
      /*update_suggestions_callback=*/base::DoNothing());
}

}  // namespace
}  // namespace autofill_prediction_improvements
