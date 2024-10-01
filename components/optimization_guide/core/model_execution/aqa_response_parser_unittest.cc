// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/aqa_response_parser.h"

#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

TEST(AqaResponseParserTest, Valid) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  auto parser = AqaResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync(
      "0001,0003 has the answer. The answer is The fox jumps over the dog",
      response_future.GetCallback());
  auto maybe_response = response_future.Get();

  ASSERT_TRUE(maybe_response.has_value());
  auto response =
      ParsedAnyMetadata<proto::HistoryAnswerResponse>(*maybe_response);
  EXPECT_EQ("The fox jumps over the dog", response->answer().text());
  EXPECT_EQ("0001", response->answer().citations()[0].passage_id());
  EXPECT_EQ("0003", response->answer().citations()[1].passage_id());
}

TEST(AqaResponseParserTest, Unanswerable) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  auto parser = AqaResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync("unanswerable.", response_future.GetCallback());
  auto maybe_response = response_future.Get();

  ASSERT_TRUE(maybe_response.has_value());
  auto response =
      ParsedAnyMetadata<proto::HistoryAnswerResponse>(*maybe_response);
  EXPECT_TRUE(response->is_unanswerable());
}

TEST(AqaResponseParserTest, RecursiveAnswer) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  auto parser = AqaResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync(
      "0001,0003 has the answer. The answer is ID:0002 has the answer. The "
      "answer is the fox jumps over the dog.",
      response_future.GetCallback());
  auto maybe_response = response_future.Get();

  EXPECT_FALSE(maybe_response.has_value());
  EXPECT_EQ(maybe_response.error(), ResponseParsingError::kFailed);
}

TEST(AqaResponseParserTest, RecursiveUnanswerable) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  auto parser = AqaResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync("0001,0003 has the answer. The answer is unanswerable.",
                     response_future.GetCallback());
  auto maybe_response = response_future.Get();

  ASSERT_TRUE(maybe_response.has_value());
  auto response =
      ParsedAnyMetadata<proto::HistoryAnswerResponse>(*maybe_response);
  EXPECT_TRUE(response->is_unanswerable());
}

TEST(AqaResponseParserTest, SuppressParsingIncompleteResponseAlwaysTrue) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.HistoryAnswerResponse");
  cfg.set_suppress_parsing_incomplete_output(false);
  auto parser = AqaResponseParserFactory().CreateParser(cfg);

  EXPECT_TRUE(parser->SuppressParsingIncompleteResponse());
}

}  // namespace optimization_guide
