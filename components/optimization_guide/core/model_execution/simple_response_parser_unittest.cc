// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/simple_response_parser.h"

#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

proto::ProtoField CreateProtoField(int single_descriptor_tag_number) {
  proto::ProtoField proto_field;
  proto_field.add_proto_descriptors()->set_tag_number(
      single_descriptor_tag_number);
  return proto_field;
}

}  // namespace

TEST(SimpleResponseParserTest, Valid) {
  SimpleResponseParser parser("optimization_guide.proto.ComposeResponse",
                              CreateProtoField(1),
                              /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(SimpleResponseParserTest, EmptyProtoField) {
  SimpleResponseParser parser("optimization_guide.proto.ComposeResponse",
                              proto::ProtoField(),
                              /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(),
            ResponseParsingError::kInvalidConfiguration);
}

TEST(SimpleResponseParserTest, BadProtoType) {
  SimpleResponseParser parser("garbage type", CreateProtoField(1),
                              /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(),
            ResponseParsingError::kInvalidConfiguration);
}

TEST(SimpleResponseParserTest, NotStringField) {
  SimpleResponseParser parser("optimization_guide.proto.ComposeResponse",
                              CreateProtoField(7),
                              /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(),
            ResponseParsingError::kInvalidConfiguration);
}

TEST(SimpleResponseParserTest, SuppressParsingIncompleteResponse) {
  for (const bool suppress_parsing_incomplete_response : {false, true}) {
    SimpleResponseParser parser("optimization_guide.proto.ComposeResponse",
                                CreateProtoField(7),
                                suppress_parsing_incomplete_response);
    EXPECT_EQ(parser.SuppressParsingIncompleteResponse(),
              suppress_parsing_incomplete_response);
  }
}

}  // namespace optimization_guide
