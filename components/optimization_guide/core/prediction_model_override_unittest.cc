// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_override.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

#if BUILDFLAG(IS_WIN)
const char kOtherAbsoluteFilePath[] = "C:\\other\\absolute\\file\\path";
#else
const char kOtherAbsoluteFilePath[] = "/other/abs/file/path";
#endif

}  // namespace

TEST(PredictionModelOverridesTest, NotSet) {
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, EmptyInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kModelOverride);
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, BadInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "whatever");
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, InvalidOptimizationTarget) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      "notanoptimizationtarget:" + std::string(kTestAbsoluteFilePath));
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, RelativeFilePath) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:" +
                                    std::string(kTestRelativeFilePath));
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, RelativeFilePathWithMetadata) {
  proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  encoded_metadata = base::Base64Encode(encoded_metadata);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestRelativeFilePath, encoded_metadata.c_str()));
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(0u, overrides.size());
}

TEST(PredictionModelOverridesTest, OneFilePath) {
  proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  encoded_metadata = base::Base64Encode(encoded_metadata);
#if BUILDFLAG(IS_WIN)
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD|%s|%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));
#else
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));
#endif

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(1u, overrides.size());
  auto* entry = overrides.Get(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->path().MaybeAsASCII(), kTestAbsoluteFilePath);
  EXPECT_EQ(entry->metadata()->type_url(), "sometypeurl");
  ASSERT_FALSE(overrides.Get(proto::OPTIMIZATION_TARGET_PAGE_TOPICS));
}

TEST(PredictionModelOverridesTest, MultipleFilePath) {
  proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  encoded_metadata = base::Base64Encode(encoded_metadata);
#if BUILDFLAG(IS_WIN)
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD|%s,"
                         "OPTIMIZATION_TARGET_PAGE_TOPICS|%s|%s",
                         kTestAbsoluteFilePath, kOtherAbsoluteFilePath,
                         encoded_metadata.c_str()));
#else
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s,"
                         "OPTIMIZATION_TARGET_PAGE_TOPICS:%s:%s",
                         kTestAbsoluteFilePath, kOtherAbsoluteFilePath,
                         encoded_metadata.c_str()));
#endif

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  EXPECT_EQ(2u, overrides.size());
  {
    auto* entry = overrides.Get(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path().MaybeAsASCII(), kTestAbsoluteFilePath);
    EXPECT_FALSE(entry->metadata());
  }
  {
    auto* entry = overrides.Get(proto::OPTIMIZATION_TARGET_PAGE_TOPICS);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path().MaybeAsASCII(), kOtherAbsoluteFilePath);
    EXPECT_EQ(entry->metadata()->type_url(), "sometypeurl");
  }
}

}  // namespace optimization_guide
