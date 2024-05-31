// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/proto/loading_predictor_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class OptimizationGuideUtilTest : public testing::Test {};

TEST_F(OptimizationGuideUtilTest, ParsedAnyMetadataMismatchedTypeTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST_F(OptimizationGuideUtilTest, ParsedAnyMetadataNotSerializableTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  any_metadata.set_value("12345678garbage");

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST_F(OptimizationGuideUtilTest, ParsedAnyMetadataTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_TRUE(parsed_metadata.has_value());
  ASSERT_EQ(parsed_metadata->subresources_size(), 1);
  const proto::Resource& parsed_subresource = parsed_metadata->subresources(0);
  EXPECT_EQ(parsed_subresource.url(), "https://example.com/");
  EXPECT_EQ(parsed_subresource.resource_type(),
            proto::ResourceType::RESOURCE_TYPE_CSS);
  EXPECT_TRUE(parsed_subresource.preconnect_only());
}

TEST_F(OptimizationGuideUtilTest, ParsedAnyMetadataTestWithNoPackageName) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/LoadingPredictorMetadata");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());

  std::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_TRUE(parsed_metadata.has_value());
  ASSERT_EQ(parsed_metadata->subresources_size(), 1);
  const proto::Resource& parsed_subresource = parsed_metadata->subresources(0);
  EXPECT_EQ(parsed_subresource.url(), "https://example.com/");
  EXPECT_EQ(parsed_subresource.resource_type(),
            proto::ResourceType::RESOURCE_TYPE_CSS);
  EXPECT_TRUE(parsed_subresource.preconnect_only());
}

}  // namespace optimization_guide
