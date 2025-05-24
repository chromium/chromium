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
using base::test::ValueIs;

proto::ExampleForTestingResponse ExtractResponseFromProtoAny(
    const proto::Any& metadata) {
  std::optional<proto::ExampleForTestingResponse> maybe_response =
      ParsedAnyMetadata<proto::ExampleForTestingResponse>(metadata);

  if (!maybe_response) {
    ADD_FAILURE() << "parsing proto::Any failed";
    return proto::ExampleForTestingResponse();
  }

  return *maybe_response;
}

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
  EXPECT_EQ(ParsedAnyMetadata<proto::ExampleForTestingResponse>(*maybe_metadata)
                ->string_value(),
            "string_output");
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

TEST(FieldwiseResponseParserTest, ParseBool) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(proto::ExampleForTestingResponse::kBoolValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\w+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<bool, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<bool, ResponseParsingError> for
    // ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.bool_value();
        });
  };

  EXPECT_THAT(parse_async("value: true"), ValueIs(true));
  EXPECT_THAT(parse_async("value: True"), ValueIs(true));
  EXPECT_THAT(parse_async("value: TRUE"), ValueIs(true));
  EXPECT_THAT(parse_async("value: false"), ValueIs(false));
  EXPECT_THAT(parse_async("value: False"), ValueIs(false));
  EXPECT_THAT(parse_async("value: FALSE"), ValueIs(false));
}

TEST(FieldwiseResponseParserTest, ParseWithFailedConversion) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(proto::ExampleForTestingResponse::kBoolValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\w+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  ParseResponseFuture response_future;
  parser.ParseAsync("value: non-bool", response_future.GetCallback());
  base::expected<proto::Any, ResponseParsingError> maybe_metadata =
      response_future.Get();

  EXPECT_THAT(maybe_metadata,
              ErrorIs(ResponseParsingError::kInvalidConfiguration));
}

TEST(FieldwiseResponseParserTest, ParseFloat) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kFloatValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<float, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<float, ResponseParsingError> for
    // ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.float_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0.0"), ValueIs(0.0f));
  EXPECT_THAT(parse_async("value: 1.0"), ValueIs(1.0f));
  EXPECT_THAT(parse_async("value: -1.0"), ValueIs(-1.0f));
  EXPECT_THAT(parse_async("value: 1000.0"), ValueIs(1000.0f));
}

TEST(FieldwiseResponseParserTest, ParseDouble) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kDoubleValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<double, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<double, ResponseParsingError>
    // for ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.double_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0.0"), ValueIs(0.0));
  EXPECT_THAT(parse_async("value: 1.0"), ValueIs(1.0));
  EXPECT_THAT(parse_async("value: -1.0"), ValueIs(-1.0));
  EXPECT_THAT(parse_async("value: 1000.0"), ValueIs(1000.0));
}

TEST(FieldwiseResponseParserTest, ParseInt32) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kInt32ValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<int32_t, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<int32_t, ResponseParsingError>
    // for ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.int32_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0"), ValueIs(0));
  EXPECT_THAT(parse_async("value: 1"), ValueIs(1));
  EXPECT_THAT(parse_async("value: -1"), ValueIs(-1));
  EXPECT_THAT(parse_async("value: 1000"), ValueIs(1000));
  // int32_t min and max.
  EXPECT_THAT(parse_async("value: -2147483648"), ValueIs(-2147483648));
  EXPECT_THAT(parse_async("value: 2147483647"), ValueIs(2147483647));
}

TEST(FieldwiseResponseParserTest, ParseUint32) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kUint32ValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<uint32_t, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<uint32_t, ResponseParsingError>
    // for ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.uint32_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0"), ValueIs(0));
  EXPECT_THAT(parse_async("value: 1"), ValueIs(1));
  EXPECT_THAT(parse_async("value: 1000"), ValueIs(1000));
  EXPECT_THAT(parse_async("value: 4294967295"), ValueIs(4294967295));
}

TEST(FieldwiseResponseParserTest, ParseInt64) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kInt64ValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<int64_t, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<int64_t, ResponseParsingError>
    // for ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.int64_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0"), ValueIs(0));
  EXPECT_THAT(parse_async("value: 1"), ValueIs(1));
  EXPECT_THAT(parse_async("value: -1"), ValueIs(-1));
  EXPECT_THAT(parse_async("value: 1000"), ValueIs(1000));
  // Values one outside the int32_t range on each side.
  EXPECT_THAT(parse_async("value: -2147483649"), ValueIs(-2147483649));
  EXPECT_THAT(parse_async("value: 2147483648"), ValueIs(2147483648));
}

TEST(FieldwiseResponseParserTest, ParseUint64) {
  proto::FieldwiseParserConfig config;
  proto::FieldExtractor* field_extractor = config.add_field_extractors();
  field_extractor->mutable_output_field()
      ->add_proto_descriptors()
      ->set_tag_number(
          proto::ExampleForTestingResponse::kUint64ValueFieldNumber);
  field_extractor->set_capturing_regex("value: (\\S+)");

  FieldwiseResponseParser parser(
      "optimization_guide.proto.ExampleForTestingResponse", config,
      /*suppress_parsing_incomplete_response=*/true);

  const auto parse_async = [&parser](const std::string& string_value)
      -> base::expected<uint64_t, ResponseParsingError> {
    ParseResponseFuture response_future;
    parser.ParseAsync(string_value, response_future.GetCallback());
    base::expected<proto::Any, ResponseParsingError> maybe_metadata =
        response_future.Get();

    // Transform the result to a base::expected<uint64_t, ResponseParsingError>
    // for ease of assertions when called.
    return maybe_metadata.transform(&ExtractResponseFromProtoAny)
        .transform([](const proto::ExampleForTestingResponse& response) {
          return response.uint64_value();
        });
  };

  EXPECT_THAT(parse_async("value: 0"), ValueIs(0));
  EXPECT_THAT(parse_async("value: 1"), ValueIs(1));
  EXPECT_THAT(parse_async("value: 1000"), ValueIs(1000));
  // Value one greater than uint32_t max.
  EXPECT_THAT(parse_async("value: 4294967296"), ValueIs(4294967296));
}

}  // namespace optimization_guide
