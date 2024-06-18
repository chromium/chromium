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

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

TEST(SimpleResponseParserTest, Valid) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.ComposeResponse");
  cfg.mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  auto parser = SimpleResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(SimpleResponseParserTest, BadProtoType) {
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("garbage type");
  cfg.mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  auto parser = SimpleResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(), ResponseParsingError::kFailed);
}

TEST(SimpleResponseParserTest, NotStringField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  proto::OnDeviceModelExecutionOutputConfig cfg;
  cfg.set_proto_type("optimization_guide.proto.ComposeResponse");
  cfg.mutable_proto_field()->add_proto_descriptors()->set_tag_number(7);
  auto parser = SimpleResponseParserFactory().CreateParser(cfg);

  ParseResponseFuture response_future;
  parser->ParseAsync("output", response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(), ResponseParsingError::kFailed);
}

}  // namespace optimization_guide
