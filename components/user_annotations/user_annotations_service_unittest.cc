// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {
namespace {

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::An;
using ::testing::IsEmpty;

class UserAnnotationsServiceTest : public testing::Test,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    InitializeFeatureList();
    CHECK(temp_dir_.CreateUniqueTempDir());
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    service_ = std::make_unique<UserAnnotationsService>(
        &model_executor_, temp_dir_.GetPath(), os_crypt_.get());
  }

  virtual void InitializeFeatureList() {
    base::FieldTrialParams feature_parameters;
    if (ShouldPersistAnnotations()) {
      feature_parameters["persist_annotations"] = "true";
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(kUserAnnotations,
                                                            feature_parameters);
  }

  bool ShouldPersistAnnotations() const { return GetParam(); }

  UserAnnotationsService* service() { return service_.get(); }

  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  std::unique_ptr<UserAnnotationsService> service_;
};

TEST_P(UserAnnotationsServiceTest, RetrieveAllEntriesNoDB) {
  base::test::TestFuture<UserAnnotationsEntries> test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_TRUE(entries.empty());
}

struct FormsAnnotationsTestRequest {
  optimization_guide::proto::Any forms_annotations_response;
  optimization_guide::proto::AXTreeUpdate ax_tree;
  autofill::FormData form_data;
};

// Returns sample annotations for tests.
FormsAnnotationsTestRequest CreateSampleFormsAnnotationsTestRequest() {
  optimization_guide::proto::FormsAnnotationsResponse response;
  optimization_guide::proto::UserAnnotationsEntry* entry1 =
      response.add_entries();
  entry1->set_key("label");
  entry1->set_value("whatever");
  optimization_guide::proto::UserAnnotationsEntry* entry2 =
      response.add_entries();
  entry2->set_key("nolabel");
  entry2->set_value("value");
  optimization_guide::proto::Any forms_annotations_response;
  forms_annotations_response.set_type_url(response.GetTypeName());
  response.SerializeToString(forms_annotations_response.mutable_value());

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  ax_tree.mutable_tree_data()->set_title("title");

  return {forms_annotations_response, ax_tree, form_data};
}

TEST_P(UserAnnotationsServiceTest, RetrieveAllEntriesWithInsert) {
  {
    base::HistogramTester histogram_tester;

    optimization_guide::proto::FormsAnnotationsRequest expected_request;
    expected_request.mutable_page_context()
        ->mutable_ax_tree_data()
        ->mutable_tree_data()
        ->set_title("title");
    expected_request.mutable_page_context()->set_title("title");
    optimization_guide::proto::FormData* form_proto =
        expected_request.mutable_form_data();
    optimization_guide::proto::FormFieldData* field_proto1 =
        form_proto->add_fields();
    field_proto1->set_field_label("label");
    field_proto1->set_field_value("whatever");
    field_proto1->set_is_visible(true);
    field_proto1->set_is_focusable(true);
    field_proto1->set_form_control_type(
        optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT);
    optimization_guide::proto::FormFieldData* field_proto2 =
        form_proto->add_fields();
    field_proto2->set_field_name("nolabel");
    field_proto2->set_field_value("value");
    field_proto2->set_is_visible(true);
    field_proto2->set_is_focusable(true);
    field_proto2->set_form_control_type(
        optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT);

    auto test_request = CreateSampleFormsAnnotationsTestRequest();
    EXPECT_CALL(
        *model_executor(),
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations,
            EqualsProto(expected_request),
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            test_request.forms_annotations_response, /*log_entry=*/nullptr));

    base::MockCallback<UserAnnotationsService::ImportFormCallback>
        import_form_callback;
    EXPECT_CALL(import_form_callback, Run(Not(IsEmpty()), _));

    service()->AddFormSubmission(test_request.ax_tree, test_request.form_data,
                                 import_form_callback.Get());

    base::test::TestFuture<UserAnnotationsEntries> test_future;
    service()->RetrieveAllEntries(test_future.GetCallback());

    auto entries = test_future.Take();
    EXPECT_EQ(2u, entries.size());

    EXPECT_EQ(entries[0].key(), "label");
    EXPECT_EQ(entries[0].value(), "whatever");
    EXPECT_EQ(entries[1].key(), "nolabel");
    EXPECT_EQ(entries[1].value(), "value");

    histogram_tester.ExpectUniqueSample(
        "UserAnnotations.AddFormSubmissionResult",
        UserAnnotationsExecutionResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;

    optimization_guide::proto::FormsAnnotationsResponse response;
    optimization_guide::proto::Any any;
    any.set_type_url(response.GetTypeName());
    response.SerializeToString(any.mutable_value());
    EXPECT_CALL(
        *model_executor(),
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(any, /*log_entry=*/nullptr));

    autofill::FormData empty_form_data;
    optimization_guide::proto::AXTreeUpdate ax_tree;
    base::MockCallback<UserAnnotationsService::ImportFormCallback>
        import_form_callback;
    EXPECT_CALL(import_form_callback, Run(IsEmpty(), _));

    service()->AddFormSubmission(ax_tree, empty_form_data,
                                 import_form_callback.Get());

    base::test::TestFuture<UserAnnotationsEntries> test_future;
    service()->RetrieveAllEntries(test_future.GetCallback());

    // Entries should still remain.
    auto entries = test_future.Take();
    EXPECT_EQ(2u, entries.size());

    EXPECT_EQ(entries[0].key(), "label");
    EXPECT_EQ(entries[0].value(), "whatever");
    EXPECT_EQ(entries[1].key(), "nolabel");
    EXPECT_EQ(entries[1].value(), "value");

    histogram_tester.ExpectUniqueSample(
        "UserAnnotations.AddFormSubmissionResult",
        UserAnnotationsExecutionResult::kSuccess, 1);
  }
}

TEST_P(UserAnnotationsServiceTest, ExecuteFailed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kGenericFailure)),
          /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::MockCallback<UserAnnotationsService::ImportFormCallback>
      import_form_callback;
  EXPECT_CALL(import_form_callback, Run(IsEmpty(), _));
  service()->AddFormSubmission(ax_tree, form_data, import_form_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseError, 1);
}

TEST_P(UserAnnotationsServiceTest, UnexpectedResponseType) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Any any;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(any, /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::MockCallback<UserAnnotationsService::ImportFormCallback>
      import_form_callback;
  EXPECT_CALL(import_form_callback, Run(IsEmpty(), _));
  service()->AddFormSubmission(ax_tree, form_data, import_form_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseMalformed, 1);
}

TEST_P(UserAnnotationsServiceTest, RemoveEntry) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          test_request.forms_annotations_response, /*log_entry=*/nullptr));
  base::MockCallback<UserAnnotationsService::ImportFormCallback>
      import_form_callback;
  EXPECT_CALL(import_form_callback, Run(Not(IsEmpty()), _));

  service()->AddFormSubmission(test_request.ax_tree, test_request.form_data,
                               import_form_callback.Get());

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future_retrieve;
  service()->RetrieveAllEntries(test_future_retrieve.GetCallback());
  auto entries = test_future_retrieve.Take();
  EXPECT_EQ(2u, entries.size());

  base::test::TestFuture<void> test_future_remove_entry;
  service()->RemoveEntry(entries[0].entry_id(),
                         test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  test_future_remove_entry.Clear();
  service()->RetrieveAllEntries(test_future_retrieve.GetCallback());
  EXPECT_TRUE(test_future_retrieve.Wait());
  EXPECT_EQ(1u, test_future_retrieve.Take().size());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveEntry.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      1);

  service()->RemoveEntry(entries[1].entry_id(),
                         test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveEntry.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      2);
  service()->RetrieveAllEntries(test_future_retrieve.GetCallback());
  EXPECT_TRUE(test_future_retrieve.Take().empty());
}

TEST_P(UserAnnotationsServiceTest, RemoveAllEntries) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          test_request.forms_annotations_response, /*log_entry=*/nullptr));
  base::MockCallback<UserAnnotationsService::ImportFormCallback>
      import_form_callback;
  EXPECT_CALL(import_form_callback, Run(Not(IsEmpty()), _));

  service()->AddFormSubmission(test_request.ax_tree, test_request.form_data,
                               import_form_callback.Get());

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future_retrieve;
  service()->RetrieveAllEntries(test_future_retrieve.GetCallback());
  EXPECT_EQ(2u, test_future_retrieve.Take().size());

  base::test::TestFuture<void> test_future_remove_entry;
  service()->RemoveAllEntries(test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveAllEntries.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      1);
  service()->RetrieveAllEntries(test_future_retrieve.GetCallback());
  EXPECT_TRUE(test_future_retrieve.Take().empty());
}

INSTANTIATE_TEST_SUITE_P(All, UserAnnotationsServiceTest, ::testing::Bool());

class UserAnnotationsServiceReplaceAnnotationsTest
    : public UserAnnotationsServiceTest {
 public:
  UserAnnotationsServiceReplaceAnnotationsTest() {
    base::FieldTrialParams feature_parameters;
    feature_parameters["should_replace_annotations_for_form_submissions"] =
        "true";
    if (ShouldPersistAnnotations()) {
      feature_parameters["persist_annotations"] = "true";
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(kUserAnnotations,
                                                            feature_parameters);
  }

 private:
  void InitializeFeatureList() override {}
};

TEST_P(UserAnnotationsServiceReplaceAnnotationsTest,
       RetrieveAllEntriesWithInsertShouldReplace) {
  {
    auto test_request = CreateSampleFormsAnnotationsTestRequest();
    EXPECT_CALL(
        *model_executor(),
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            test_request.forms_annotations_response, /*log_entry=*/nullptr));
    base::MockCallback<UserAnnotationsService::ImportFormCallback>
        import_form_callback;
    EXPECT_CALL(import_form_callback, Run(Not(IsEmpty()), _));

    service()->AddFormSubmission(test_request.ax_tree, test_request.form_data,
                                 import_form_callback.Get());

    base::test::TestFuture<UserAnnotationsEntries> test_future;
    service()->RetrieveAllEntries(test_future.GetCallback());

    auto entries = test_future.Take();
    EXPECT_EQ(2u, entries.size());

    EXPECT_EQ(entries[0].key(), "label");
    EXPECT_EQ(entries[0].value(), "whatever");
    EXPECT_EQ(entries[1].key(), "nolabel");
    EXPECT_EQ(entries[1].value(), "value");
  }

  {
    optimization_guide::proto::FormsAnnotationsResponse response;
    optimization_guide::proto::Any any;
    any.set_type_url(response.GetTypeName());
    response.SerializeToString(any.mutable_value());
    EXPECT_CALL(
        *model_executor(),
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(any, /*log_entry=*/nullptr));
    autofill::FormData empty_form_data;
    optimization_guide::proto::AXTreeUpdate ax_tree;
    base::MockCallback<UserAnnotationsService::ImportFormCallback>
        import_form_callback;
    EXPECT_CALL(import_form_callback, Run(IsEmpty(), _));

    service()->AddFormSubmission(ax_tree, empty_form_data,
                                 import_form_callback.Get());

    base::test::TestFuture<UserAnnotationsEntries> test_future;
    service()->RetrieveAllEntries(test_future.GetCallback());

    // Entries should be cleared since there were no fields to replace with.
    auto entries = test_future.Take();
    EXPECT_EQ(0u, entries.size());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserAnnotationsServiceReplaceAnnotationsTest,
                         ::testing::Bool());

}  // namespace
}  // namespace user_annotations
