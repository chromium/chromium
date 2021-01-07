// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_metadata.h"

#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(OptimizationMetadataTest, ParsedMetadataAnyMetadataNotPopulatedTest) {
  OptimizationMetadata optimization_metadata;

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataNoTypeURLTest) {
  proto::Any any_metadata;
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  dase_metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataMismatchedTypeTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  dase_metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataNotSerializableTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.DelayAsyncScriptExecutionMetadata");
  any_metadata.set_value("12345678garbage");
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationMetadataTest, ParsedMetadataTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.DelayAsyncScriptExecutionMetadata");
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  dase_metadata.SerializeToString(any_metadata.mutable_value());
  OptimizationMetadata optimization_metadata;
  optimization_metadata.set_any_metadata(any_metadata);

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_TRUE(parsed_dase_metadata.has_value());
  EXPECT_EQ(parsed_dase_metadata->delay_type(),
            proto::DELAY_TYPE_FINISHED_PARSING);
}

TEST(OptimizationMetadataTest, SetAnyMetadataForTestingTest) {
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  OptimizationMetadata optimization_metadata;
  optimization_metadata.SetAnyMetadataForTesting(dase_metadata);

  base::Optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          optimization_metadata
              .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  EXPECT_TRUE(parsed_dase_metadata.has_value());
  EXPECT_EQ(parsed_dase_metadata->delay_type(),
            proto::DELAY_TYPE_FINISHED_PARSING);
}

}  // namespace optimization_guide
