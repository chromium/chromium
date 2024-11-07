// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
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
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_annotations/user_annotations_switches.h"
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

class UserAnnotationsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    InitializeFeatureList();
    CHECK(temp_dir_.CreateUniqueTempDir());
    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();
    logs_service_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(&local_state_);
    service_ = std::make_unique<UserAnnotationsService>(
        &model_executor_, temp_dir_.GetPath(), os_crypt_.get(),
        optimization_guide_decider_.get());
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitAndEnableFeature(autofill_ai::kAutofillAi);
  }

  UserAnnotationsEntries AddAndImportFormSubmission(
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      const autofill::FormData& form_data) {
    UserAnnotationsEntries entries;
    std::unique_ptr<autofill::FormStructure> form =
        std::make_unique<autofill::FormStructure>(form_data);
    service()->AddFormSubmission(
        GURL("example.com"), "title", ax_tree_update, std::move(form),
        base::BindLambdaForTesting(
            [&entries](std::unique_ptr<autofill::FormStructure> form,
                       std::unique_ptr<user_annotations::FormAnnotationResponse>
                           form_annotation_response,
                       PromptAcceptanceCallback prompt_acceptance_callback) {
              if (form_annotation_response) {
                entries = form_annotation_response->to_be_upserted_entries;
              }
              std::move(prompt_acceptance_callback)
                  .Run({/*prompt_was_accepted=*/true,
                        /*did_user_interact=*/true});
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
        logs_service_->GetWeakPtr());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_service_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<UserAnnotationsService> service_;
};

TEST_F(UserAnnotationsServiceTest, FormsAnnotationsTypeRegistered) {
  EXPECT_TRUE(base::Contains(
      optimization_guide_decider()->registered_optimization_types(),
      optimization_guide::proto::FORMS_ANNOTATIONS));
}

TEST_F(UserAnnotationsServiceTest, ShouldAddFormSubmissionForURL) {
  EXPECT_FALSE(service()->ShouldAddFormSubmissionForURL(
      GURL("https://notallowed.com/whatever")));
  EXPECT_TRUE(service()->ShouldAddFormSubmissionForURL(
      GURL("https://allowed.com/whatever")));
  // Allowed host but not HTTPS.
  EXPECT_FALSE(service()->ShouldAddFormSubmissionForURL(
      GURL("http://allowed.com/whatever")));
}

TEST_F(UserAnnotationsServiceTest, RetrieveAllEntriesNoDB) {
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

  return {optimization_guide::AnyWrapProto(response), ax_tree, form_data,
          GURL("example.com"), "title"};
}

TEST_F(UserAnnotationsServiceTest, RetrieveAllEntriesWithInsert) {
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
            EqualsProto(expected_request), _,
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<3>(
            optimization_guide::OptimizationGuideModelExecutionResult(
                test_request.forms_annotations_response, nullptr),
            CreateLogEntry()));

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
    EXPECT_CALL(
        *model_executor(),
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _,
            _,
            An<optimization_guide::
                   OptimizationGuideModelExecutionResultCallback>()))
        .WillOnce(base::test::RunOnceCallback<3>(
            optimization_guide::OptimizationGuideModelExecutionResult(
                optimization_guide::AnyWrapProto(response), nullptr),
            CreateLogEntry()));

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

TEST_F(UserAnnotationsServiceTest, ExecuteFailed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          optimization_guide::
                              OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure)),
              nullptr),
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

TEST_F(UserAnnotationsServiceTest, UnexpectedResponseType) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Any any;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(any,
                                                                    nullptr),
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
      UserAnnotationsExecutionResult::kResponseMalformed, 1);

  EXPECT_TRUE(logs_service()->uploaded_logs().empty());
}

TEST_F(UserAnnotationsServiceTest, RemoveEntry) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              test_request.forms_annotations_response, nullptr),
          CreateLogEntry()));

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

TEST_F(UserAnnotationsServiceTest, RemoveAllEntries) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              test_request.forms_annotations_response, nullptr),
          CreateLogEntry()));

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

TEST_F(UserAnnotationsServiceTest, FormNotImported) {
  base::HistogramTester histogram_tester;
  auto test_request = CreateSampleFormsAnnotationsTestRequest();
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              test_request.forms_annotations_response, nullptr),
          CreateLogEntry()));

  service()->AddFormSubmission(
      test_request.url, test_request.title, test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(test_request.form_data),
      base::BindLambdaForTesting(
          [](std::unique_ptr<autofill::FormStructure> form,
             std::unique_ptr<user_annotations::FormAnnotationResponse>
                 form_annotation_response,
             PromptAcceptanceCallback prompt_acceptance_callback) {
            std::move(prompt_acceptance_callback)
                .Run({/*prompt_was_accepted=*/false});
          }));

  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

TEST_F(UserAnnotationsServiceTest, ParallelFormSubmissions) {
  base::HistogramTester histogram_tester;
  auto first_test_request = CreateSampleFormsAnnotationsTestRequest();
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      first_execute_callback,
      second_execute_callback;
  PromptAcceptanceCallback first_prompt_acceptance_callback,
      second_prompt_acceptance_callback;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(MoveArg<3>(&first_execute_callback))
      .WillOnce(MoveArg<3>(&second_execute_callback));

  service()->AddFormSubmission(
      first_test_request.url, first_test_request.title,
      first_test_request.ax_tree,
      std::make_unique<autofill::FormStructure>(first_test_request.form_data),
      base::BindLambdaForTesting(
          [&first_prompt_acceptance_callback](
              std::unique_ptr<autofill::FormStructure> form,
              std::unique_ptr<user_annotations::FormAnnotationResponse>
                  form_annotation_response,
              PromptAcceptanceCallback callback) {
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
              std::unique_ptr<user_annotations::FormAnnotationResponse>
                  form_annotation_response,
              PromptAcceptanceCallback callback) {
            second_prompt_acceptance_callback = std::move(callback);
          }));

  // Only the first model execute should happen.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_execute_callback);
  EXPECT_FALSE(second_execute_callback);
  std::move(first_execute_callback)
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               first_test_request.forms_annotations_response, nullptr),
           CreateLogEntry());

  // Only the first prompt acceptance call should happen.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(second_execute_callback);
  EXPECT_TRUE(first_prompt_acceptance_callback);
  std::move(first_prompt_acceptance_callback)
      .Run({/*prompt_was_accepted=*/true, /*did_user_interact=*/true});
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
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               second_test_request.forms_annotations_response, nullptr),
           CreateLogEntry());
  task_environment_.RunUntilIdle();
  std::move(second_prompt_acceptance_callback)
      .Run({/*prompt_was_accepted=*/true, /*did_user_interact=*/true});

  histogram_tester.ExpectUniqueSample("UserAnnotations.AddFormSubmissionResult",
                                      UserAnnotationsExecutionResult::kSuccess,
                                      2);

  entries = GetAllUserAnnotationsEntries();
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(entries[0].key(), "label");
  EXPECT_EQ(entries[0].value(), "new_value");
  EXPECT_EQ(entries[1].key(), "nolabel");
  EXPECT_EQ(entries[1].value(), "new_nolabel_value");
}

TEST_F(UserAnnotationsServiceTest, SaveAutofillProfile) {
  autofill::AutofillProfile autofill_profile(AddressCountryCode("US"));
  autofill::test::SetProfileInfo(&autofill_profile, "Jane", "J", "Doe",
                                 "jd@example.com", "", "123 Main St", "",
                                 "Raleigh", "NC", "12345", "US", "9195555555");
  base::test::TestFuture<UserAnnotationsExecutionResult> test_future;
  service()->SaveAutofillProfile(autofill_profile, test_future.GetCallback());
  ASSERT_TRUE(test_future.Wait());
  const UserAnnotationsEntries entries = GetAllUserAnnotationsEntries();
  EXPECT_EQ(entries.size(), 10u);
  EXPECT_EQ(entries[0].key(), "First Name");
  EXPECT_EQ(entries[0].value(), "Jane");
  EXPECT_EQ(entries[1].key(), "Middle Name");
  EXPECT_EQ(entries[1].value(), "J");
  EXPECT_EQ(entries[2].key(), "Last Name");
  EXPECT_EQ(entries[2].value(), "Doe");
  EXPECT_EQ(entries[3].key(), "Email Address");
  EXPECT_EQ(entries[3].value(), "jd@example.com");
  EXPECT_EQ(entries[4].key(), "Phone Number [mobile]");
  EXPECT_EQ(entries[4].value(), "9195555555");
  EXPECT_EQ(entries[5].key(), "Address - City");
  EXPECT_EQ(entries[5].value(), "Raleigh");
  EXPECT_EQ(entries[6].key(), "Address - State");
  EXPECT_EQ(entries[6].value(), "NC");
  EXPECT_EQ(entries[7].key(), "Address - Zip Code");
  EXPECT_EQ(entries[7].value(), "12345");
  EXPECT_EQ(entries[8].key(), "Address - Country");
  EXPECT_EQ(entries[8].value(), "US");
  EXPECT_EQ(entries[9].key(), "Address - Street");
  EXPECT_EQ(entries[9].value(), "123 Main St");
}

class UserAnnotationsServiceSeededAnnotationTest
    : public UserAnnotationsServiceTest {
 public:
  void SetUp() override {
    const std::vector<Entry>& response_upserted_entries = {
        {0, "label", "whatever"},
        {0, "nolabel", "value"},
    };
    optimization_guide::proto::FormsAnnotationsResponse response;
    for (const auto& entry : response_upserted_entries) {
      optimization_guide::proto::UserAnnotationsEntry* new_entry =
          response.add_upserted_entries();
      new_entry->set_entry_id(entry.entry_id);
      new_entry->set_key(entry.key);
      new_entry->set_value(entry.value);
    }

    std::string encoded_annotations;
    response.SerializeToString(&encoded_annotations);
    encoded_annotations = base::Base64Encode(encoded_annotations);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kFormsAnnotationsOverride, encoded_annotations);

    UserAnnotationsServiceTest::SetUp();
  }
};

TEST_F(UserAnnotationsServiceSeededAnnotationTest, SeedAnnotations) {
  task_environment_.RunUntilIdle();
  auto entries = GetAllUserAnnotationsEntries();
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(entries[0].key(), "label");
  EXPECT_EQ(entries[0].value(), "whatever");
  EXPECT_EQ(entries[1].key(), "nolabel");
  EXPECT_EQ(entries[1].value(), "value");
}

}  // namespace
}  // namespace user_annotations
