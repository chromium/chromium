// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/safety_checker.h"

#include <cstdint>
#include <initializer_list>
#include <sstream>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::ResultOf;

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}
const google::protobuf::RepeatedField<float>& GetScores(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.response().text_safety_model_response().scores();
}
bool GetIsUnsafe(const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.response().text_safety_model_response().is_unsafe();
}

proto::ComposeRequest UrlAndInputRequest(const std::string& url,
                                         const std::string& input) {
  proto::ComposeRequest req;
  req.mutable_page_metadata()->set_page_url(url);
  req.mutable_generate_params()->set_user_input(input);
  return req;
}

proto::Any SimpleResponse(const std::string& output) {
  proto::ComposeResponse resp;
  resp.set_output(output);
  return AnyWrapProto(resp);
}

}  // namespace

class FakeTextSafetyClient : public TextSafetyClient {
 public:
  FakeTextSafetyClient() {
    CHECK(ts_data_.Create());
    CHECK(ts_sp_model_.Create());
    CHECK(language_model_.Create());
    CHECK(base::WriteFile(ts_data_.path(), on_device_model::FakeTsData()));
    CHECK(
        base::WriteFile(ts_sp_model_.path(), on_device_model::FakeTsSpModel()));
    CHECK(base::WriteFile(language_model_.path(),
                          on_device_model::FakeLanguageModel()));
    fake_ts_model_.emplace(TsParams());
    receiver_set_.Add(&fake_ts_model_.value(),
                      remote_.BindNewPipeAndPassReceiver());
  }

  on_device_model::mojom::TextSafetyModelParamsPtr TsParams() {
    on_device_model::TextSafetyLoaderParams params;
    params.ts_paths.emplace();
    params.ts_paths->data = ts_data_.path();
    params.ts_paths->sp_model = ts_sp_model_.path();
    params.language_paths.emplace();
    params.language_paths->model = language_model_.path();
    return LoadTextSafetyParams(params);
  }

  mojo::Remote<on_device_model::mojom::TextSafetyModel>&
  GetTextSafetyModelRemote() override {
    return remote_;
  }

 private:
  base::ScopedTempFile ts_data_;
  base::ScopedTempFile ts_sp_model_;
  base::ScopedTempFile language_model_;
  std::optional<on_device_model::FakeTsModel> fake_ts_model_;
  mojo::ReceiverSet<on_device_model::mojom::TextSafetyModel> receiver_set_;
  mojo::Remote<on_device_model::mojom::TextSafetyModel> remote_;
};

class SafetyCheckerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  FakeTextSafetyClient client_;
  base::test::TestFuture<SafetyChecker::Result> future_;
};

TEST(SafetyConfigTest, MissingScoreIsUnsafe) {
  auto safety_config = ComposeSafetyConfig();
  auto* threshold = safety_config.add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1};  // Only 1 score, but expects 2.
  EXPECT_TRUE(cfg.IsUnsafeText(safety_info));
}

TEST(SafetyConfigTest, SafeWithRequiredScores) {
  auto safety_config = ComposeSafetyConfig();
  auto* threshold = safety_config.add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1, 0.1};  // Has score with index = 1.
  EXPECT_FALSE(cfg.IsUnsafeText(safety_info));
}

TEST_F(SafetyCheckerTest, RawOutputCheckPassesWithTrivialConfig) {
  // When no thresholds are defined, all outputs will pass.
  SafetyChecker checker([]() { return SafetyConfig(ComposeSafetyConfig()); }());
  checker.RunRawOutputCheck(client_, "unsafe raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(
                  ResultOf("check text", &GetCheckText, "unsafe raw output"),
                  ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                  ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, DefaultOutputSafetyPassesOnSafeOutput) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    // No explicit raw_output_check, should still perform a default one.
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "reasonable raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText, "reasonable raw output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, DefaultOutputSafetyFailsOnUnsafeOutput) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    // No explicit raw_output_check, should still perform a default one.
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "unsafe raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(
                  ResultOf("check text", &GetCheckText, "unsafe raw output"),
                  ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                  ResultOf("is_unsafe", &GetIsUnsafe, true))));
}

TEST_F(SafetyCheckerTest, OutputSafetyPassesWithMetRequiredLanguage) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "reasonable raw output in esperanto",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText,
                   "is_raw_output_safe: reasonable raw output in esperanto"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, OutputSafetyFailsWithUnmetRequiredLanguage) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "reasonable raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: reasonable raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest,
       OutputSafetyPassesWithSafeOutputAndNoRequiredLanguage) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "reasonable raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: reasonable raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, OutputSafetyFailsWithUnsafeOutput) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return SafetyConfig(safety_config);
  }());
  checker.RunRawOutputCheck(client_, "unsafe raw output",
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: unsafe raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, true))));
}

TEST_F(SafetyCheckerTest, RequestChecksPassesWithTrivialConfig) {
  SafetyChecker checker([]() { return SafetyConfig(ComposeSafetyConfig()); }());
  checker.RunRequestChecks(client_,
                           UrlAndInputRequest("unsafe_url", "unsafe_input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs, ElementsAre());
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithSafeUrl) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_url: %s", PageUrlField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    {
      auto* check2 = safety_config.add_request_check();
      check2->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_,
                           UrlAndInputRequest("safe_url", "reasonable input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText, "is_safe_url: safe_url"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, false)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "is_safe_input: reasonable input"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnsafeUrl) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_url: %s", PageUrlField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    {
      auto* check2 = safety_config.add_request_check();
      check2->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_,
                           UrlAndInputRequest("unsafe_url", "reasonable input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_url: unsafe_url"),
                        ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, true)),
                  AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_input: reasonable input"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnmetRequiredLanguage) {
  SafetyChecker checker([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_, UserInputRequest("english input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText, "is_safe_input: english input"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest,
       RequestCheckPassesWithUnmetRequiredLanguageButIgnored) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_, UserInputRequest("english input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText, "is_safe_input: english input"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithMetRequiredLanguage) {
  SafetyChecker checker([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_, UserInputRequest("esperanto input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_input: esperanto input"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithLanguageOnly) {
  SafetyChecker checker([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      // Note: these thresholds are ignored.
      check->mutable_safety_category_thresholds()->Add(RequireReasonable());
      check->set_check_language_only(true);
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_, UserInputRequest("esperanto input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                         "is_safe_input: esperanto input"),
                                ResultOf("scores", &GetScores, IsEmpty()),
                                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithLanguageOnly) {
  SafetyChecker checker([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      // Note: these thresholds are ignored.
      check->mutable_safety_category_thresholds()->Add(RequireReasonable());
      check->set_check_language_only(true);
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunRequestChecks(client_, UserInputRequest("english input"),
                           future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                         "is_safe_input: english input"),
                                ResultOf("scores", &GetScores, IsEmpty()),
                                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, ResponseCheckPassesWithSafeResponse) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunResponseChecks(
      client_, UrlAndInputRequest("very_", "reasonable_esperanto_"),
      SimpleResponse("safe_output"), future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check: very_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, false)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check2: reasonable_esperanto_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnsafeResponse) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunResponseChecks(
      client_, UrlAndInputRequest("un", "reasonable_esperanto_"),
      SimpleResponse("safe_output"), future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check: unsafe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, true)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check2: reasonable_esperanto_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, ResponseCheckFailsWithUnmetRequiredLanguge) {
  SafetyChecker checker([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return SafetyConfig(safety_config);
  }());
  checker.RunResponseChecks(client_, UrlAndInputRequest("very_", "reasonable_"),
                            SimpleResponse("safe_output"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "response_check: very_safe_output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false)),
                  AllOf(ResultOf("check text", &GetCheckText,
                                 "response_check2: reasonable_safe_output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

}  // namespace optimization_guide
