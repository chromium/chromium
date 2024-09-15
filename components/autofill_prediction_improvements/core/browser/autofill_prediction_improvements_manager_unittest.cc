// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "components/user_annotations/user_annotations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;
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

MATCHER_P(HasType, expected_type, "") {
  EXPECT_THAT(arg, Field(&Suggestion::type, Eq(expected_type)));
  return true;
}

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
  MOCK_METHOD(user_annotations::UserAnnotationsService*,
              GetUserAnnotationsService,
              (),
              (override));
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
  autofill::TestStrikeDatabase strike_database_;

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
    ON_CALL(client_, GetUserAnnotationsService)
        .WillByDefault(Return(&user_annotations_service_));
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_, &strike_database_);
  }

 protected:
  user_annotations::TestUserAnnotationsService user_annotations_service_;
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;
};

TEST_F(AutofillPredictionImprovementsManagerTest, RejctedPromptStrikeCounting) {
  autofill::FormStructure form1{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(1));

  autofill::FormStructure form2{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(2));

  // Neither of the forms should be blocked in the beginning.
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // After up to two strikes the form should not blocked.
  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // After the third strike form1 should become blocked but form2 remains
  // unblocked.
  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // Now the second form received three strikes and gets eventually blocked.
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form2));

  // After resetting form2, form1 should remain blocked.
  manager_->RemoveStrikesForImportFromForm(form2);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
}

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
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> filling_suggestion;

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

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  ASSERT_THAT(
      filling_suggestion,
      ElementsAre(HasType(SuggestionType::kFillPredictionImprovements),
                  HasType(SuggestionType::kPredictionImprovementsFeedback)));
  const Suggestion::PredictionImprovementsPayload filling_payload =
      filling_suggestion[0]
          .GetPayload<Suggestion::PredictionImprovementsPayload>();
  const autofill::FormFieldData& filled_field = filled_form.fields().front();
  EXPECT_THAT(
      filling_payload.values_to_fill,
      ElementsAre(Pair(filled_field.global_id(), filled_field.value())));
  EXPECT_THAT(
      filling_suggestion[0].children,
      ElementsAre(HasType(SuggestionType::kFillPredictionImprovements),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kFillPredictionImprovements)));
}

// Tests that no suggestions are added to `address_suggestions` if
// `should_add_trigger_suggestion` is `false`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsDoesNotUpdateIfItShouldNot) {
  std::vector<Suggestion> address_suggestions;
  autofill::FormFieldData field;
  EXPECT_FALSE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/false));
}

// Tests that `address_suggestions` only contains the
// triggering improved predictions suggestions if it was empty before calling
// `MaybeUpdateSuggestions()`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsOnEmptyAddressSuggestionsAddsTriggerSuggestion) {
  std::vector<Suggestion> address_suggestions;
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(HasType(SuggestionType::kRetrievePredictionImprovements),
                  HasType(SuggestionType::kPredictionImprovementsDetails)));
}

// Tests that `address_suggestions` contains the
// triggering improved prediction suggestions (incl. a separator) if
// `address_suggestions` contained entries before calling
// `MaybeUpdateSuggestions()`. The pre-existing entries contained a
// `kUndoOrClear` suggestion which is the case for autofilled fields.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    MaybeUpdateSuggestionsOnAddressSuggestionsIncludingUndoOrClearAddsTriggerSuggestion) {
  std::vector<Suggestion> address_suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kUndoOrClear),
      Suggestion(SuggestionType::kManageAddress)};
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(HasType(SuggestionType::kAddressEntry),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kRetrievePredictionImprovements),
                  HasType(SuggestionType::kPredictionImprovementsDetails),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kUndoOrClear),
                  HasType(SuggestionType::kManageAddress)));
}

// Tests that `address_suggestions` contains the
// improved prediction suggestions (incl. a separator) if
// `address_suggestions` contained entries before calling
// `MaybeUpdateSuggestions()`. The pre-existing entries did not contain a
// `kUndoOrClear` suggestion which is the case for fields that aren't
// autofilled.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeUpdateSuggestionsOnAddressSuggestionsAddsTriggerSuggestion) {
  std::vector<Suggestion> address_suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageAddress)};
  autofill::FormFieldData field;
  EXPECT_TRUE(manager_->MaybeUpdateSuggestions(
      address_suggestions, field, /*should_add_trigger_suggestion=*/true));
  EXPECT_THAT(
      address_suggestions,
      ElementsAre(HasType(SuggestionType::kAddressEntry),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kRetrievePredictionImprovements),
                  HasType(SuggestionType::kPredictionImprovementsDetails),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kManageAddress)));
}

class AutofillPredictionImprovementsManagerImportFormTest
    : public AutofillPredictionImprovementsManagerTest,
      public testing::WithParamInterface<bool> {};

// Tests that `import_form_callback` is run with added entries if the import was
// successful.
TEST_P(AutofillPredictionImprovementsManagerImportFormTest,
       MaybeImportFormRunsCallbackWithAddedEntriesWhenImportWasSuccessful) {
  user_annotations_service_.AddHostToFormAnnotationsAllowlist(url_.host());
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_data = autofill::test::GetFormData(form_description);
  autofill::FormStructure eligible_form_structure(form_data);
  autofill::FormStructureTestApi form_test_api(eligible_form_structure);

  autofill::AutofillField& prediction_improvement_field =
      form_test_api.PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  prediction_improvement_field.set_heuristic_type(
      autofill::HeuristicSource::kPredictionImprovementRegexes,
      autofill::IMPROVED_PREDICTION);
#else
  prediction_improvement_field.set_heuristic_type(
      autofill::GetActiveHeuristicSource(), autofill::IMPROVED_PREDICTION);
#endif
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::ImportFormCallback>
      import_form_callback;
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  EXPECT_CALL(client_, GetAXTree)
      .WillOnce(MoveArg<0>(&axtree_received_callback));
  user_annotations_service_.SetShouldImportFormData(
      /*should_import_form_data=*/GetParam());

  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      user_annotations_entries;
  EXPECT_CALL(import_form_callback, Run)
      .WillOnce(SaveArg<0>(&user_annotations_entries));
  manager_->MaybeImportForm(form_data, eligible_form_structure,
                            import_form_callback.Get());
  std::move(axtree_received_callback).Run({});
  EXPECT_THAT(user_annotations_entries.empty(), !GetParam());
}

INSTANTIATE_TEST_SUITE_P(,
                         AutofillPredictionImprovementsManagerImportFormTest,
                         testing::Bool());

// Tests that `import_form_callback` is run with an empty list of entries when
// `user_annotations::ShouldAddFormSubmissionForURL()` returns `false`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeImportFormRunsCallbackWithFalseWhenImportIsNotAttempted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      user_annotations::kUserAnnotations,
      {{"allowed_hosts_for_form_submissions", "otherhost.com"}});
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::ImportFormCallback>
      import_form_callback;

  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      user_annotations_entries;
  EXPECT_CALL(import_form_callback, Run)
      .WillOnce(SaveArg<0>(&user_annotations_entries));
  manager_->MaybeImportForm(/*form=*/{},
                            /*form_structure=*/autofill::FormStructure({}),
                            import_form_callback.Get());
  EXPECT_TRUE(user_annotations_entries.empty());
}

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(true)` if there's data stored in the user annotations.
TEST_F(AutofillPredictionImprovementsManagerTest,
       HasDataStoredReturnsTrueIfDataIsStored) {
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::HasDataCallback>
      has_data_callback;
  user_annotations_service_.ReplaceAllEntries(
      {optimization_guide::proto::UserAnnotationsEntry()});
  manager_->HasDataStored(has_data_callback.Get());
  EXPECT_CALL(
      has_data_callback,
      Run(autofill::AutofillPredictionImprovementsDelegate::HasData(true)));
  manager_->HasDataStored(has_data_callback.Get());
}

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(false)` if there's no data stored in the user annotations.
TEST_F(AutofillPredictionImprovementsManagerTest,
       HasDataStoredReturnsFalseIfDataIsNotStored) {
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::HasDataCallback>
      has_data_callback;
  user_annotations_service_.ReplaceAllEntries({});
  manager_->HasDataStored(has_data_callback.Get());
  EXPECT_CALL(
      has_data_callback,
      Run(autofill::AutofillPredictionImprovementsDelegate::HasData(false)));
  manager_->HasDataStored(has_data_callback.Get());
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
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> error_suggestion;
  EXPECT_CALL(client_, GetAXTree).Times(0);
  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&error_suggestion));
  }

  manager.OnClickedTriggerSuggestion(form_, form_.fields().front(),
                                     update_suggestions_callback.Get());

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  EXPECT_THAT(error_suggestion,
              ElementsAre(HasType(SuggestionType::kAutocompleteEntry)));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfDeciderIsNull) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, nullptr,
                                                &strike_database_};
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> error_suggestion;
  EXPECT_CALL(client_, GetAXTree).Times(0);
  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&error_suggestion));
  }

  manager.OnClickedTriggerSuggestion(form_, form_.fields().front(),
                                     update_suggestions_callback.Get());

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  EXPECT_THAT(error_suggestion,
              ElementsAre(HasType(SuggestionType::kAutocompleteEntry)));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       ExtractsImprovedPredictionsIfSkipAllowlistIsTrue) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  EXPECT_CALL(client_, GetAXTree);
  EXPECT_CALL(update_suggestions_callback, Run)
      .WillOnce(SaveArg<0>(&loading_suggestion));

  manager.OnClickedTriggerSuggestion(form_, form_.fields().front(),
                                     update_suggestions_callback.Get());

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfOptimizationGuideCannotBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> error_suggestion;
  EXPECT_CALL(client_, GetAXTree).Times(0);
  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&error_suggestion));
  }

  manager.OnClickedTriggerSuggestion(form_, form_.fields().front(),
                                     update_suggestions_callback.Get());

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  EXPECT_THAT(error_suggestion,
              ElementsAre(HasType(SuggestionType::kAutocompleteEntry)));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       ExtractsImprovedPredictionsIfOptimizationGuideCanBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  EXPECT_CALL(client_, GetAXTree);
  EXPECT_CALL(update_suggestions_callback, Run)
      .WillOnce(SaveArg<0>(&loading_suggestion));

  manager.OnClickedTriggerSuggestion(form_, form_.fields().front(),
                                     update_suggestions_callback.Get());

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       IsFormEligible_EmptyForm) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});

  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  EXPECT_FALSE(manager.IsFormEligible(form));
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       IsFormEligible_EligibleForm) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});

  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  autofill::AutofillField& prediction_improvement_field =
      form_test_api.PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  prediction_improvement_field.set_heuristic_type(
      autofill::HeuristicSource::kPredictionImprovementRegexes,
      autofill::IMPROVED_PREDICTION);
#else
  prediction_improvement_field.set_heuristic_type(
      autofill::HeuristicSource::kLegacyRegexes, autofill::IMPROVED_PREDICTION);
#endif

  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  EXPECT_TRUE(manager.IsFormEligible(form));
}
}  // namespace
}  // namespace autofill_prediction_improvements
