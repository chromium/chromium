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
using ::testing::Eq;
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
  MockOptimizationGuideDecider decider_;
  MockAutofillPredictionImprovementsFillingEngine filling_engine_;
  MockAutofillPredictionImprovementsClient client_;
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

// Tests that the callback delivering improved predictions is called eventually.
TEST_F(AutofillPredictionImprovementsManagerTest,
       ExtractImprovedPredictionsForFormFields) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  form_description.fields[0].value = u"John";
  autofill::FormData filled_form =
      autofill::test::GetFormData(form_description);
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;

  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::FillPredictionsCallback>
      fill_callback;

  EXPECT_CALL(client_, GetAXTree)
      .WillOnce(MoveArg<0>(&axtree_received_callback));
  EXPECT_CALL(filling_engine_, GetPredictions)
      .WillOnce(MoveArg<2>(&predictions_received_callback));

  EXPECT_CALL(fill_callback, Run);
  manager_->ExtractImprovedPredictionsForFormFields(form, fill_callback.Get());
  std::move(axtree_received_callback).Run({});
  std::move(predictions_received_callback).Run(filled_form);
}

class ShouldProvideAutofillPredictionImprovementsTest
    : public BaseAutofillPredictionImprovementsManagerTest {
 public:
  ShouldProvideAutofillPredictionImprovementsTest() {
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
  }

 protected:
  autofill::FormData form_;
};

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfFlagDisabled) {
  feature_.InitAndDisableFeature(kAutofillPredictionImprovements);
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager.ExtractImprovedPredictionsForFormFields(form_, base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       DoesNotExtractImprovedPredictionsIfDeciderIsNull) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, nullptr};
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager.ExtractImprovedPredictionsForFormFields(form_, base::DoNothing());
}

TEST_F(ShouldProvideAutofillPredictionImprovementsTest,
       ExtractsImprovedPredictionsIfSkipAllowlistIsTrue) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_};
  EXPECT_CALL(client_, GetAXTree);
  manager.ExtractImprovedPredictionsForFormFields(form_, base::DoNothing());
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
  manager.ExtractImprovedPredictionsForFormFields(form_, base::DoNothing());
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
  manager.ExtractImprovedPredictionsForFormFields(form_, base::DoNothing());
}

}  // namespace
}  // namespace autofill_prediction_improvements
