// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/parser_kind.pb.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

class MockResponseParser : public ResponseParser {
 public:
  MOCK_METHOD(void,
              ParseAsync,
              (const std::string& redacted_output,
               ResultCallback result_callback),
              (const override));
  MOCK_METHOD(bool, SuppressParsingIncompleteResponse, (), (const override));
};

OnDeviceModelFeatureAdapter::ResponseParserFactory
CreateMockResponseParserFactory(
    base::RepeatingCallback<void(MockResponseParser&)> apply_expectations) {
  const auto factory_function =
      [](base::RepeatingCallback<void(MockResponseParser&)> apply_expectations,
         const proto::OnDeviceModelExecutionOutputConfig& config)
      -> std::unique_ptr<ResponseParser> {
    auto parser = std::make_unique<MockResponseParser>();
    apply_expectations.Run(*parser);
    return parser;
  };
  return base::BindRepeating(factory_function, apply_expectations);
}

using testing::Return;

}  // namespace

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestNoSafetyFallbackConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(
                              MultimodalMessageReadView(request), "text"));
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestNoUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.mutable_text_safety_fallback_config();
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  auto safety_request = adapter->ConstructTextSafetyRequest(
      MultimodalMessageReadView(request), "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_TRUE(safety_request->url().empty());
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestWithUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(3);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(1);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  auto safety_request = adapter->ConstructTextSafetyRequest(
      MultimodalMessageReadView(request), "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_EQ("url", safety_request->url());
}

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestWithBadUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(
                              MultimodalMessageReadView(request), "text"));
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_NoInputConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_MismatchRequest) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("wrong name");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_ForInputContext) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* ctx_substitution = input_config->add_input_context_substitutions();
  ctx_substitution->set_string_template("hello this is %s");
  ctx_substitution->add_substitutions()->add_candidates()->set_raw_string(
      "input context");
  auto* execute_substitution = input_config->add_execute_substitutions();
  execute_substitution->set_string_template("this should be ignored");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is input context");
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_ForExecution) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* ctx_substitution = input_config->add_input_context_substitutions();
  ctx_substitution->set_string_template("this should be ignored");
  auto* execute_substitution = input_config->add_execute_substitutions();
  execute_substitution->set_string_template("hello this is %s");
  execute_substitution->add_substitutions()->add_candidates()->set_raw_string(
      "execution");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is execution");
}

TEST(OnDeviceModelFeatureAdapterTest, ParseResponse) {
  auto adapter = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
      proto::OnDeviceModelExecutionFeatureConfig(),
      CreateMockResponseParserFactory(
          base::BindRepeating([](MockResponseParser& parser) {
            EXPECT_CALL(parser, ParseAsync)
                .WillOnce([](const std::string& redacted_output,
                             ResponseParser::ResultCallback result_callback) {
                  proto::Any any;
                  any.set_value(redacted_output + " response");
                  std::move(result_callback).Run(any);
                });
          })));

  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output", 0u, response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_any =
      response_future.Get();

  ASSERT_TRUE(maybe_any.has_value());
  EXPECT_EQ("output response", maybe_any.value().value());
}

TEST(OnDeviceModelFeatureAdapterTest, ParseResponse_NullParser) {
  auto adapter = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
      proto::OnDeviceModelExecutionFeatureConfig(),
      base::BindRepeating(
          [](const proto::OnDeviceModelExecutionOutputConfig& config) {
            return std::unique_ptr<ResponseParser>();
          }));

  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output", 0u, response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_any =
      response_future.Get();

  ASSERT_FALSE(maybe_any.has_value());
  EXPECT_EQ(ResponseParsingError::kFailed, maybe_any.error());
}

TEST(OnDeviceModelFeatureAdapterTest, ShouldParseResponseCompleteOnly) {
  auto adapter = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
      proto::OnDeviceModelExecutionFeatureConfig(),
      CreateMockResponseParserFactory(
          base::BindRepeating([](MockResponseParser& parser) {
            EXPECT_CALL(parser, SuppressParsingIncompleteResponse)
                .WillRepeatedly(Return(true));
          })));

  EXPECT_FALSE(adapter->ShouldParseResponse(ResponseCompleteness::kPartial));
  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kComplete));
}

TEST(OnDeviceModelFeatureAdapterTest, ShouldParseResponseAlways) {
  auto adapter = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
      proto::OnDeviceModelExecutionFeatureConfig(),
      CreateMockResponseParserFactory(
          base::BindRepeating([](MockResponseParser& parser) {
            EXPECT_CALL(parser, SuppressParsingIncompleteResponse)
                .WillRepeatedly(Return(false));
          })));

  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kPartial));
  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kComplete));
}

}  // namespace optimization_guide
