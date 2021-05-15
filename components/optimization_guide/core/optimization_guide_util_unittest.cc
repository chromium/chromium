// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataMismatchedTypeTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  dase_metadata.SerializeToString(any_metadata.mutable_value());

  absl::optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          ParsedAnyMetadata<proto::DelayAsyncScriptExecutionMetadata>(
              any_metadata);
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataNotSerializableTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.DelayAsyncScriptExecutionMetadata");
  any_metadata.set_value("12345678garbage");

  absl::optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          ParsedAnyMetadata<proto::DelayAsyncScriptExecutionMetadata>(
              any_metadata);
  EXPECT_FALSE(parsed_dase_metadata.has_value());
}

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.DelayAsyncScriptExecutionMetadata");
  proto::DelayAsyncScriptExecutionMetadata dase_metadata;
  dase_metadata.set_delay_type(proto::DELAY_TYPE_FINISHED_PARSING);
  dase_metadata.SerializeToString(any_metadata.mutable_value());

  absl::optional<proto::DelayAsyncScriptExecutionMetadata>
      parsed_dase_metadata =
          ParsedAnyMetadata<proto::DelayAsyncScriptExecutionMetadata>(
              any_metadata);
  EXPECT_TRUE(parsed_dase_metadata.has_value());
  EXPECT_EQ(parsed_dase_metadata->delay_type(),
            proto::DELAY_TYPE_FINISHED_PARSING);
}

}  // namespace optimization_guide
