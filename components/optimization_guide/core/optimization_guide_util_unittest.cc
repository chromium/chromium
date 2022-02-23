// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/proto/loading_predictor_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataMismatchedTypeTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());

  absl::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataNotSerializableTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  any_metadata.set_value("12345678garbage");

  absl::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_FALSE(parsed_metadata.has_value());
}

TEST(OptimizationGuideUtilTest, ParsedAnyMetadataTest) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");
  proto::LoadingPredictorMetadata metadata;
  proto::Resource* subresource = metadata.add_subresources();
  subresource->set_url("https://example.com/");
  subresource->set_resource_type(proto::ResourceType::RESOURCE_TYPE_CSS);
  subresource->set_preconnect_only(true);
  metadata.SerializeToString(any_metadata.mutable_value());

  absl::optional<proto::LoadingPredictorMetadata> parsed_metadata =
      ParsedAnyMetadata<proto::LoadingPredictorMetadata>(any_metadata);
  EXPECT_TRUE(parsed_metadata.has_value());
  ASSERT_EQ(parsed_metadata->subresources_size(), 1);
  const proto::Resource& parsed_subresource = parsed_metadata->subresources(0);
  EXPECT_EQ(parsed_subresource.url(), "https://example.com/");
  EXPECT_EQ(parsed_subresource.resource_type(),
            proto::ResourceType::RESOURCE_TYPE_CSS);
  EXPECT_TRUE(parsed_subresource.preconnect_only());
}

#if !BUILDFLAG(IS_WIN)

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetSwitchNotSet) {
  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
  EXPECT_FALSE(switches::IsModelOverridePresent());
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetEmptyInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kModelOverride);

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(OptimizationGuideUtilTest, GetModelOverrideForOptimizationTargetBadInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "whatever");

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetInvalidOptimizationTarget) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      "notanoptimizationtarget:" + std::string(kTestAbsoluteFilePath));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetRelativeFilePath) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:" +
                                    std::string(kTestRelativeFilePath));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetRelativeFilePathWithMetadata) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestRelativeFilePath, encoded_metadata.c_str()));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetOneFilePath) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(kTestAbsoluteFilePath, file_path_and_metadata->first);
  EXPECT_EQ("sometypeurl", file_path_and_metadata->second->type_url());
}

TEST(OptimizationGuideUtilTest,
     GetModelOverrideForOptimizationTargetMultipleFilePath) {
  const char kOtherAbsoluteFilePath[] = "/other/file/path";
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s,"
                         "OPTIMIZATION_TARGET_PAGE_TOPICS:%s:%s",
                         kTestAbsoluteFilePath, kOtherAbsoluteFilePath,
                         encoded_metadata.c_str()));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_EQ(kTestAbsoluteFilePath, file_path_and_metadata->first);

  file_path_and_metadata = GetModelOverrideForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS);
  EXPECT_EQ(kOtherAbsoluteFilePath, file_path_and_metadata->first);
  EXPECT_EQ("sometypeurl", file_path_and_metadata->second->type_url());
}

#endif

}  // namespace optimization_guide
