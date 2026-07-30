// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/json_generation.h"

#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kAppKey[] = "application";
constexpr char kAppName[] = "Chrome";
constexpr char kArmProcessor[] = "arm64";
constexpr char kBetaChannel[] = "Beta";
constexpr char kCohortName[] = "cohort1";
constexpr char kDevChannel[] = "Dev";
constexpr char kLinuxOS[] = "Linux";
constexpr char kManagedStatus[] = "Managed";
constexpr char kMetadataAppPath[] = "chromeMetadata.application";
constexpr char kPlatformDesktop[] = "Desktop";
constexpr char kPolicyPath[] = "chromePolicies.PolicyTest";
constexpr char kPolicyValue[] = "ValueTest";
constexpr char kStableChannel[] = "Stable";
constexpr char kStatusPath[] = "status.device.status";
constexpr char kStatusValuePath[] = "device.status";
constexpr char kX64Processor[] = "64-bit";

}  // namespace

TEST(JsonGenerationTest, GetChromeMetadataValueAllFields) {
  JsonGenerationParams params;
  params.with_application_name(kAppName)
      .with_channel_name(kDevChannel)
      .with_processor_variation(kX64Processor)
      .with_cohort_name(kCohortName)
      .with_os_name(kLinuxOS)
      .with_platform_name(kPlatformDesktop);

  base::DictValue metadata = GetChromeMetadataValue(params);

  base::DictValue expected_subset;
  expected_subset.Set(kAppKey, kAppName);
  expected_subset.Set(kChromeMetadataOSKey, kLinuxOS);
  expected_subset.Set(kChromeMetadataPlatformKey, kPlatformDesktop);
  expected_subset.Set(kChromeMetadataRevisionKey,
                      version_info::GetLastChange());
  EXPECT_THAT(metadata, base::test::IsSupersetOfValue(expected_subset));

  const std::string* version = metadata.FindString(kChromeMetadataVersionKey);
  ASSERT_TRUE(version);
  EXPECT_NE(std::string::npos, version->find(version_info::GetVersionNumber()));
  EXPECT_NE(std::string::npos, version->find(kDevChannel));
  EXPECT_NE(std::string::npos, version->find(kX64Processor));
  EXPECT_NE(std::string::npos, version->find(kCohortName));
}

TEST(JsonGenerationTest, GetChromeMetadataValueOptionalFieldsEmpty) {
  JsonGenerationParams params;
  params.with_application_name(kAppName)
      .with_channel_name(kBetaChannel)
      .with_processor_variation(kArmProcessor);

  base::DictValue metadata = GetChromeMetadataValue(params);

  base::DictValue expected_subset;
  expected_subset.Set(kAppKey, kAppName);
  expected_subset.Set(kChromeMetadataRevisionKey,
                      version_info::GetLastChange());
  EXPECT_THAT(metadata, base::test::IsSupersetOfValue(expected_subset));
  EXPECT_FALSE(metadata.FindString(kChromeMetadataOSKey));
  EXPECT_FALSE(metadata.FindString(kChromeMetadataPlatformKey));
}

TEST(JsonGenerationTest, GenerateJson) {
  JsonGenerationParams params;
  params.with_application_name(kAppName).with_channel_name(kStableChannel);

  base::DictValue policy_values;
  policy_values.SetByDottedPath(kPolicyPath, kPolicyValue);

  base::DictValue status_values;
  status_values.SetByDottedPath(kStatusValuePath, kManagedStatus);

  std::string json_str =
      GenerateJson(std::move(policy_values), std::move(status_values), params);
  EXPECT_FALSE(json_str.empty());

  base::DictValue expected;
  expected.SetByDottedPath(kMetadataAppPath, kAppName);
  expected.SetByDottedPath(kPolicyPath, kPolicyValue);
  expected.SetByDottedPath(kStatusPath, kManagedStatus);
  EXPECT_THAT(base::test::ParseJson(json_str),
              base::test::IsSupersetOfValue(expected));
}

}  // namespace policy
