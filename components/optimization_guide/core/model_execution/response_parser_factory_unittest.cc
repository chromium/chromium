// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/response_parser_factory.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "components/optimization_guide/proto/parser_kind.pb.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

// base::expected GMock matcher.
using base::test::HasValue;
using base::test::ValueIs;

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

// Check that the simple parser is appropriately selected.
TEST(CreateResponseParserTest, SimpleResponseParser) {
  proto::OnDeviceModelExecutionOutputConfig config;
  config.set_parser_kind(proto::PARSER_KIND_SIMPLE);
  config.set_proto_type("optimization_guide.proto.ComposeResponse");
  config.mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  std::unique_ptr<ResponseParser> parser = CreateResponseParser(config);

  ParseResponseFuture response_future;
  parser->ParseAsync("output", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

// Check that the JSON parser is appropriately selected.
TEST(CreateResponseParserTest, JsonResponseParser) {
  base::test::TaskEnvironment task_environment;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  proto::OnDeviceModelExecutionOutputConfig config;
  config.set_parser_kind(proto::PARSER_KIND_JSON);
  config.set_proto_type("optimization_guide.proto.ComposeResponse");
  std::unique_ptr<ResponseParser> parser = CreateResponseParser(config);

  ParseResponseFuture response_future;
  parser->ParseAsync("{\"output\": \"abc\"}", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  EXPECT_THAT(
      "abc",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(CreateResponseParserTest, AqaResponseParser) {
  proto::OnDeviceModelExecutionOutputConfig config;
  config.set_parser_kind(proto::PARSER_KIND_AQA);
  config.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  std::unique_ptr<ResponseParser> parser = CreateResponseParser(config);

  ParseResponseFuture response_future;
  parser->ParseAsync(
      "0001,0003 has the answer. The answer is The fox jumps over the dog",
      response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  auto response =
      ParsedAnyMetadata<proto::HistoryAnswerResponse>(*maybe_metadata);
  EXPECT_EQ("The fox jumps over the dog", response->answer().text());
  EXPECT_EQ("0001", response->answer().citations()[0].passage_id());
  EXPECT_EQ("0003", response->answer().citations()[1].passage_id());
}

}  // namespace

}  // namespace optimization_guide
