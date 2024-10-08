// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
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

class TestOptimizationGuideDecider
    : public optimization_guide::TestOptimizationGuideDecider {
 public:
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    CHECK_EQ(optimization_type, optimization_guide::proto::FORMS_ANNOTATIONS);
    if (url.host() == "allowed.com") {
      return optimization_guide::OptimizationGuideDecision::kTrue;
    }
    return optimization_guide::OptimizationGuideDecision::kFalse;
  }
};

class UserAnnotationsServiceTest : public testing::Test,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    InitializeFeatureList();
    CHECK(temp_dir_.CreateUniqueTempDir());
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();
    logs_service_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        /*pref_service=*/nullptr);
    service_ = std::make_unique<UserAnnotationsService>(
        &model_executor_, temp_dir_.GetPath(), os_crypt_.get(),
        optimization_guide_decider_.get());
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

  UserAnnotationsEntries AddAndImportFormSubmission(
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      const autofill::FormData& form_data) {
    UserAnnotationsEntries entries;
    std::unique_ptr<autofill::FormStructure> form =
        std::make_unique<autofill::FormStructure>(form_data);
    service()->AddFormSubmission(
        GURL("example.com"), "title", ax_tree_update, std::move(form),
        base::BindLambdaForTesting(
            [&entries](
                std::unique_ptr<autofill::FormStructure> form,
                UserAnnotationsEntries upserted_entries,
                base::OnceCallback<void(bool)> prompt_acceptance_callback) {
              entries = upserted_entries;
              std::move(prompt_acceptance_callback).Run(true);
            }));
    task_environment_.RunUntilIdle();
    return entries;
  }

  UserAnnotationsEntries GetAllUserAnnotationsEntries() {
    base::test::TestFuture<UserAnnotationsEntries> test_future;
    service()->RetrieveAllEntries(test_future.GetCallback());
    return test_future.Take();
  }

  UserAnnotationsService* service() { return service_.get(); }

  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

  TestOptimizationGuideDecider* optimization_guide_decider() {
    return optimization_guide_decider_.get();
  }

  optimization_guide::TestModelQualityLogsUploaderService* logs_service() {
    return logs_service_.get();
  }

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> CreateLogEntry() {
    return std::make_unique<optimization_guide::ModelQualityLogEntry>(
        std::make_unique<optimization_guide::proto::LogAiDataRequest>(),
        logs_service_->GetWeakPtr());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_service_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<UserAnnotationsService> service_;
};

TEST_P(UserAnnotationsServiceTest, FormsAnnotationsTypeRegistered) {
  EXPECT_TRUE(base::Contains(
      optimization_guide_decider()->registered_optimization_types(),
      optimization_guide::proto::FORMS_ANNOTATIONS));
}

TEST_P(UserAnnotationsServiceTest, ShouldAddFormSubmissionForURL) {
  EXPECT_FALSE(service()->ShouldAddFormSubmissionForURL(
      GURL("https://notallowed.com/whatever")));
  EXPECT_TRUE(service()->ShouldAddFormSubmissionForURL(
      GURL("https://allowed.com/whatever")));
}

TEST_P(UserAnnotationsServiceTest, RetrieveAllEntriesNoDB) {
  auto entries = GetAllUserAnnotationsEntries();
  EXPECT_TRUE(entries.empty());
}

struct FormsAnnotationsTestRequest {
  optimization_guide::proto::Any forms_annotations_response;
  optimization_guide::proto::AXTreeUpdate ax_tree;
  autofill::FormData form_data;
  GURL url;
  std::string title;
};

struct Entry {
  size_t entry_id;
  std::string key;
  std::string value;
};

// Returns sample annotations for tests.
FormsAnnotationsTestRequest CreateSampleFormsAnnotationsTestRequest(
    const std::vector<std::vector<std::u16string>>& request_entries =
        {
            {u"label", u"", u"whatever"},
            {u"", u"nolabel", u"value"},
        },
    const std::vector<Entry>& response_upserted_entries = {
        {0, "label", "whatever"},
        {0, "nolabel", "value"},
    }) {
  optimization_guide::proto::FormsAnnotationsResponse response;
  for (const auto& entry : response_upserted_entries) {
    optimization_guide::proto::UserAnnotationsEntry* new_entry =
        response.add_upserted_entries();
    new_entry->set_entry_id(entry.entry_id);
    new_entry->set_key(entry.key);
    new_entry->set_value(entry.value);
  }
  optimization_guide::proto::Any forms_annotations_response;
  forms_annotations_response.set_type_url(response.GetTypeName());
  response.SerializeToString(forms_annotations_response.mutable_value());

  std::vector<autofill::FormFieldData> form_fields;
  for (const auto& entry : request_entries) {
    autofill::FormFieldData form_field;
    form_field.set_label(entry[0]);
    form_field.set_name(entry[1]);
    form_field.set_value(entry[2]);
    form_fields.push_back(form_field);
  }
  autofill::FormData form_data;
  form_data.set_fields(form_fields);
  optimization_guide::proto::AXTreeUpdate ax_tree;
  ax_tree.mutable_tree_data()->set_title("title");

  return {forms_annotations_response, ax_tree, form_data, GURL("example.com"),
          "title"};
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
            test_request.forms_annotations_response, CreateLogEntry()));

    EXPECT_FALSE(
        AddAndImportFormSubmission(test_request.ax_tree, test_request.form_data)
            .empty());

    auto entries = GetAllUserAnnotationsEntries();
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
        .WillOnce(base::test::RunOnceCallback<2>(any, CreateLogEntry()));

    autofill::FormData empty_form_data;
    optimization_guide::proto::AXTreeUpdate ax_tree;

    EXPECT_TRUE(AddAndImportFormSubmission(ax_tree, empty_form_data).empty());

    // Entries should still remain.
    auto entries = GetAllUserAnnotationsEntries();
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
          CreateLogEntry()));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;

  EXPECT_TRUE(AddAndImportFormSubmission(ax_tree, form_data).empty());

  histogram_tester.ExpectUniqueSample(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseError, 1);

  EXPECT_TRUE(logs_service()->uploaded_logs().empty());
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
      .WillOnce(base::test::RunOnceCallback<2>(any, CreateLogEntry()));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  EXPECT_TRUE(AddAndImportFormSubmission(ax_tree, form_data).empty());

  histogram_tester.ExpectUniqueSample(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseMalformed, 1);

  EXPECT_TRUE(logs_service()->uploaded_logs().empty());
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
          test_request.forms_annotations_response, CreateLogEntry()));

  EXPECT_FALSE(
      AddAndImportFormSubmission(test_request.ax_tree, test_request.form_data)
          .empty());

  auto entries = GetAllUserAnnotationsEntries();
  EXPECT_EQ(2u, entries.size());

  base::test::TestFuture<void> test_future_remove_entry;
  service()->RemoveEntry(entries[0].entry_id(),
                         test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  test_future_remove_entry.Clear();
  EXPECT_EQ(1u, GetAllUserAnnotationsEntries().size());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveEntry.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      1);

  service()->RemoveEntry(entries[1].entry_id(),
                         test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveEntry.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      2);
  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
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
          test_request.forms_annotations_response, CreateLogEntry()));

  EXPECT_FALSE(
      AddAndImportFormSubmission(test_request.ax_tree, test_request.form_data)
          .empty());

  EXPECT_EQ(2u, GetAllUserAnnotationsEntries().size());

  base::test::TestFuture<void> test_future_remove_entry;
  service()->RemoveAllEntries(test_future_remove_entry.GetCallback());
  EXPECT_TRUE(test_future_remove_entry.Wait());
  histogram_tester.ExpectUniqueSample("UserAnnotations.RemoveAllEntries.Result",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      1);
  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

TEST_P(UserAnnotationsServiceTest, FormNotImported) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          test_request.forms_annotations_response, CreateLogEntry()));

  service()->AddFormSubmission(
      test_request.url, test_request.title, test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(test_request.form_data),
      base::BindLambdaForTesting(
          [](std::unique_ptr<autofill::FormStructure> form,
             UserAnnotationsEntries upserted_entries,
             base::OnceCallback<void(bool)> prompt_acceptance_callback) {
            std::move(prompt_acceptance_callback).Run(false);
          }));

  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

TEST_P(UserAnnotationsServiceTest, FormImportTimeout) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          test_request.forms_annotations_response, CreateLogEntry()));

  base::OnceCallback<void(bool)> prompt_acceptance_callback;
  service()->AddFormSubmission(
      test_request.url, test_request.title, test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(test_request.form_data),
      base::BindLambdaForTesting(
          [&prompt_acceptance_callback](
              std::unique_ptr<autofill::FormStructure> form,
              UserAnnotationsEntries upserted_entries,
              base::OnceCallback<void(bool)> callback) {
            // Don't drop or run the callback.
            prompt_acceptance_callback = std::move(callback);
          }));
  task_environment_.FastForwardBy(base::Minutes(1));

  // Success will be recorded when the model execution succeeded, and timed-out
  // will be recorded when the import callback times out.
  histogram_tester.ExpectBucketCount(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseTimedOut, 1);
  histogram_tester.ExpectBucketCount("UserAnnotations.AddFormSubmissionResult",
                                     UserAnnotationsExecutionResult::kSuccess,
                                     1);
  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

TEST_P(UserAnnotationsServiceTest, ModelExecuteTimeout) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  // Let the model execute timeout without calling the result callback.
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .Times(1);

  service()->AddFormSubmission(
      test_request.url, test_request.title, test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(test_request.form_data),
      base::BindLambdaForTesting(
          [](std::unique_ptr<autofill::FormStructure> form,
             UserAnnotationsEntries upserted_entries,
             base::OnceCallback<void(bool)> callback) {
            EXPECT_TRUE(upserted_entries.empty());
          }));
  task_environment_.FastForwardBy(base::Minutes(1));

  histogram_tester.ExpectUniqueSample(
      "UserAnnotations.AddFormSubmissionResult",
      UserAnnotationsExecutionResult::kResponseTimedOut, 1);
  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

TEST_P(UserAnnotationsServiceTest, ParallelFormSubmissions) {
  base::HistogramTester histogram_tester;
  auto first_test_request = CreateSampleFormsAnnotationsTestRequest();
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      first_execute_callback,
      second_execute_callback;
  base::OnceCallback<void(bool)> first_prompt_acceptance_callback,
      second_prompt_acceptance_callback;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(MoveArg<2>(&first_execute_callback))
      .WillOnce(MoveArg<2>(&second_execute_callback));

  service()->AddFormSubmission(
      first_test_request.url, first_test_request.title,
      first_test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(first_test_request.form_data),
      base::BindLambdaForTesting(
          [&first_prompt_acceptance_callback](
              std::unique_ptr<autofill::FormStructure> form,
              UserAnnotationsEntries upserted_entries,
              base::OnceCallback<void(bool)> callback) {
            first_prompt_acceptance_callback = std::move(callback);
          }));

  auto second_test_request =
      CreateSampleFormsAnnotationsTestRequest(/*request_entries=*/
                                              {
                                                  {u"label", u"", u"whatever"},
                                                  {u"", u"nolabel", u"value"},
                                              },
                                              /*response_upserted_entries=*/{
                                                  {1, "label", "new_value"},
                                                  {2, "nolabel",
                                                   "new_nolabel_value"},
                                              });

  service()->AddFormSubmission(
      second_test_request.url, second_test_request.title,
      second_test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(second_test_request.form_data),
      base::BindLambdaForTesting(
          [&second_prompt_acceptance_callback](
              std::unique_ptr<autofill::FormStructure> form,
              UserAnnotationsEntries upserted_entries,
              base::OnceCallback<void(bool)> callback) {
            second_prompt_acceptance_callback = std::move(callback);
          }));

  // Only the first model execute should happen.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_execute_callback);
  EXPECT_FALSE(second_execute_callback);
  std::move(first_execute_callback)
      .Run(first_test_request.forms_annotations_response, CreateLogEntry());

  // Only the first prompt acceptance call should happen.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(second_execute_callback);
  EXPECT_TRUE(first_prompt_acceptance_callback);
  std::move(first_prompt_acceptance_callback).Run(true);
  histogram_tester.ExpectUniqueSample("UserAnnotations.AddFormSubmissionResult",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      1);

  auto entries = GetAllUserAnnotationsEntries();
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(entries[0].key(), "label");
  EXPECT_EQ(entries[0].value(), "whatever");
  EXPECT_EQ(entries[1].key(), "nolabel");
  EXPECT_EQ(entries[1].value(), "value");

  // Now the second form submission should happen.
  task_environment_.RunUntilIdle();
  std::move(second_execute_callback)
      .Run(second_test_request.forms_annotations_response, CreateLogEntry());
  task_environment_.RunUntilIdle();
  std::move(second_prompt_acceptance_callback).Run(true);

  histogram_tester.ExpectUniqueSample("UserAnnotations.AddFormSubmissionResult",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      2);

  entries = GetAllUserAnnotationsEntries();

  if (ShouldPersistAnnotations()) {
    EXPECT_EQ(2u, entries.size());
    EXPECT_EQ(entries[0].key(), "label");
    EXPECT_EQ(entries[0].value(), "new_value");
    EXPECT_EQ(entries[1].key(), "nolabel");
    EXPECT_EQ(entries[1].value(), "new_nolabel_value");
  } else {
    // In the in-memory entries case, the entries are always added.
    EXPECT_EQ(4u, entries.size());
    EXPECT_EQ(entries[0].key(), "label");
    EXPECT_EQ(entries[0].value(), "whatever");
    EXPECT_EQ(entries[1].key(), "nolabel");
    EXPECT_EQ(entries[1].value(), "value");
    EXPECT_EQ(entries[2].key(), "label");
    EXPECT_EQ(entries[2].value(), "new_value");
    EXPECT_EQ(entries[3].key(), "nolabel");
    EXPECT_EQ(entries[3].value(), "new_nolabel_value");
  }
}

INSTANTIATE_TEST_SUITE_P(All, UserAnnotationsServiceTest, ::testing::Bool());

}  // namespace
}  // namespace user_annotations
