// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_metadata.h"

#include "components/optimization_guide/proto/loading_predictor_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(OptimizationMetadataTest, ParsedMetadataAnyMetadataNotPopulatedTest) {
  OptimizationMetadata optimization_metadata;

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataNoTypeURLTest) {
  proto::Any any_metadata;
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataMismatchedTypeTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataNotSerializableTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  any_metadata.set_value("12345678garbage");
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_TRUE(parsed_metadata.has_value());
  ASSERT_EQ(parsed_metadata->subresources_size(), 1);
  const proto::Resource& parsed_subresource = parsed_metadata->subresources(0);
  EXPECT_EQ(parsed_subresource.url(), "https://example.com/");
  EXPECT_EQ(parsed_subresource.resource_type(),
            proto::ResourceType::RESOURCE_TYPE_CSS);
  EXPECT_TRUE(parsed_subresource.preconnect_only());
}

TEST(OptimizationMetadataTest, SetAnyMetadataForTestingTest) {
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  OptimizationMetadata optimization_metadata;
  optimization_metadata.SetAnyMetadataForTesting(metadata);

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>();
  EXPECT_TRUE(parsed_metadata.has_value());
  ASSERT_EQ(parsed_metadata->subresources_size(), 1);
  const proto::Resource& parsed_subresource = parsed_metadata->subresources(0);
  EXPECT_EQ(parsed_subresource.url(), "https://example.com/");
  EXPECT_EQ(parsed_subresource.resource_type(),
            proto::ResourceType::RESOURCE_TYPE_CSS);
  EXPECT_TRUE(parsed_subresource.preconnect_only());
}

}  // namespace optimization_guide
