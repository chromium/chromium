// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/fieldwise_response_parser.h"

#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

// base::expected GMock matchers.
using base::test::ErrorIs;
using base::test::HasValue;

}  // namespace

TEST(FieldwiseResponseParserTest, ParseWithNoOutputField) {
  proto::FieldwiseParserConfig config;
  config.add_field_extractors()->set_capturing_regex("a");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());

  EXPECT_THAT(response_future.Get(),
              ErrorIs(ResponseParsingError::kInvalidConfiguration));
}

TEST(FieldwiseResponseParserTest, ParseWithNoCapturingRegex) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kStringValueFieldNumber);

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("output", response_future.GetCallback());

  EXPECT_THAT(response_future.Get(),
              ErrorIs(ResponseParsingError::kInvalidConfiguration));
}

TEST(FieldwiseResponseParserTest, ParseWithNonMatchingRegex) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kStringValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\w+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("non-matching string", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  // We expect the value to be unset, i.e. the default.
  EXPECT_EQ("",
            ParsedAnyMetadata<proto::ExampleForTestingResponse>(*maybe_metadata)
                ->string_value());
}

TEST(FieldwiseResponseParserTest, ParseString) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kStringValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\w+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("value: string_output", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  EXPECT_EQ("string_output",
            ParsedAnyMetadata<proto::ExampleForTestingResponse>(*maybe_metadata)
                ->string_value());
}

TEST(FieldwiseResponseParserTest, ParseWithTranslation) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kStringValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\w+)");
  (*field_extractor->mutable_translation_map())["string_output"] =
      "translated_output";

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("value: string_output", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  EXPECT_EQ("translated_output",
            ParsedAnyMetadata<proto::ExampleForTestingResponse>(*maybe_metadata)
                ->string_value());
}

TEST(FieldwiseResponseParserTest, MultipleExtractors) {
  const auto make_field_extractor = [](const std::string& regex) {
    proto::FieldExtractor field_extractor;
    field_extractor.mutable_output_field()
        ->add_proto_descriptors()
        ->set_tag_number(
            proto::ExampleForTestingResponse::kStringValueFieldNumber);
    field_extractor.set_capturing_regex(regex);
    return field_extractor;
  };

  proto::FieldwiseParserConfig config;
  *config.add_field_extractors() = make_field_extractor("value: (a)b");
  *config.add_field_extractors() = make_field_extractor("value: a(b)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);
  ParseResponseFuture response_future;
  parser.ParseAsync("value: ab", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  ASSERT_THAT(maybe_metadata, HasValue());
  // The second extractor should run and overwrite the string_value field
  // written by the first extractor.
  EXPECT_EQ("b",
            ParsedAnyMetadata<proto::ExampleForTestingResponse>(*maybe_metadata)
                ->string_value());
}

}  // namespace optimization_guide
