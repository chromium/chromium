// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {

namespace {

using ::testing::Eq;
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

}  // namespace

class AutofillPredictionImprovementsManagerTest : public testing::Test {
 public:
  AutofillPredictionImprovementsManagerTest() {
    ON_CALL(client_, GetFillingEngine)
        .WillByDefault(testing::Return(&filling_engine_));
    manager_ =
        std::make_unique<AutofillPredictionImprovementsManager>(&client_);
  }

 protected:
  MockAutofillPredictionImprovementsFillingEngine filling_engine_;
  MockAutofillPredictionImprovementsClient client_;
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
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

}  // namespace autofill_prediction_improvements
