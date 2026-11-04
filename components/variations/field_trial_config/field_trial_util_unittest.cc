// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <map>
#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// TODO(crbug.com/40742801): Remove when fake VariationsServiceClient created.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

class FieldTrialUtilTest : public ::testing::Test {
 public:
  FieldTrialUtilTest() = default;

  FieldTrialUtilTest(const FieldTrialUtilTest&) = delete;
  FieldTrialUtilTest& operator=(const FieldTrialUtilTest&) = delete;

  ~FieldTrialUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    test::ClearAllVariationIDs();
    test::ClearAllVariationParams();
  }

 protected:
  TestVariationsServiceClient variation_service_client_;
};

}  // namespace

TEST_F(FieldTrialUtilTest, AssociateParamsFromString) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.B:a/%2F";
  ASSERT_TRUE(AssociateParamsFromString(kVariationsString));

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("/", base::GetFieldTrialParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), base::GetFieldTrialParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), base::GetFieldTrialParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("/", params["a"]);
}

TEST_F(FieldTrialUtilTest, AssociateParamsFromStringWithSameTrial) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.A:a/x";
  ASSERT_FALSE(AssociateParamsFromString(kVariationsString));
}

TEST_F(FieldTrialUtilTest, AssociateParamsFromFieldTrialConfig) {
  const Study::Platform platform = Study::PLATFORM_LINUX;
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial1",
                    .experiments = {{.name = "TestGroup1",
                                     .platforms = {platform},
                                     .params = {{"x", "1"}, {"y", "2"}}}}},
                   {.name = "TestTrial2",
                    .experiments = {{.name = "TestGroup2",
                                     .platforms = {platform},
                                     .params = {{"x", "1"}, {"y", "2"}}},
                                    {.name = "TestGroup2-2",
                                     .platforms = {platform},
                                     .params = {{"x", "3"}, {"y", "4"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial1", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial1", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial1", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("TestGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
}

// Verifies that studies in the field trial config should be skipped if they
// enable/disable features that were overridden through the command line.
TEST_F(FieldTrialUtilTest, FieldTrialConfigSkipOverridden) {
  const Study::Platform platform = Study::PLATFORM_LINUX;
  base::FeatureList feature_list;
  // Enable feature "A" and disable feature "B" as if they were enabled/disabled
  // using the |--enable-features| and |--disable-features| switches.
  base::FieldTrialList::CreateFieldTrial("Study", "Experiment");
  feature_list.InitFromCommandLine(/*enable_features=*/"A<Study",
                                   /*disable_features=*/"B");

  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial0",
                    .experiments = {{.name = "TestGroup0",
                                     .platforms = {platform},
                                     .enable_features = {"A"}}}},
                   {.name = "TestTrial1",
                    .experiments = {{.name = "TestGroup1",
                                     .platforms = {platform},
                                     .disable_features = {"B"}}}},
                   {.name = "TestTrial2",
                    .experiments = {{.name = "TestGroup2",
                                     .platforms = {platform},
                                     .enable_features = {"C"},
                                     .disable_features = {"D"}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  // Expect only TestTrial2 to have been registered as it is the only study to
  // not enable/disable features A or B.
  EXPECT_FALSE(base::FieldTrialList::TrialExists("TestTrial0"));
  EXPECT_FALSE(base::FieldTrialList::TrialExists("TestTrial1"));
  EXPECT_TRUE(base::FieldTrialList::TrialExists("TestTrial2"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithEachPlatform) {
  const Study::Platform all_platforms[] = {
      Study::PLATFORM_ANDROID,  // Comment to prevent clang format bin packing.
      Study::PLATFORM_ANDROID_DESKTOP,
      Study::PLATFORM_ANDROID_WEBVIEW,
      Study::PLATFORM_CHROMEOS,
      Study::PLATFORM_FUCHSIA,
      Study::PLATFORM_IOS,
      Study::PLATFORM_LINUX,
      Study::PLATFORM_MAC,
      Study::PLATFORM_WINDOWS,
  };

  // Break if platforms are added without updating |all_platforms|.
  // Study::Platform has 2 reserved enum values (8 and 9).
  static_assert(std::size(all_platforms) == Study::Platform_ARRAYSIZE - 2,
                "|all_platforms| must include all platforms.");

  for (Study::Platform platform : all_platforms) {
    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        {.studies = {{.name = "TestTrial",
                      .experiments = {{.name = "TestGroup",
                                       .platforms = {platform},
                                       .params = {{"x", "1"}, {"y", "2"}}}}}}},
        platform, variation_service_client_.GetCurrentFormFactor(),
        &feature_list);

    EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
    EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

    std::map<std::string, std::string> params;
    EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
    EXPECT_EQ(2U, params.size());
    EXPECT_EQ("1", params["x"]);
    EXPECT_EQ("2", params["y"]);

    EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
  }
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithDifferentPlatform) {
  // The platforms don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {Study::PLATFORM_ANDROID},
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMultiplePlatforms) {
  // One of the platforms matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial",
             .experiments = {{.name = "TestGroup",
                              .platforms = {Study::PLATFORM_ANDROID,
                                            Study::PLATFORM_ANDROID_WEBVIEW},
                              .params = {{"x", "1"}, {"y", "2"}}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithAllFormFactors) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments =
                        {{.name = "TestGroup",
                          .platforms = {platform},
                          .form_factors = {Study::DESKTOP, Study::PHONE,
                                           Study::TABLET, Study::MEET_DEVICE,
                                           Study::FOLDABLE},
                          .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithSingleFormFactor) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const Study::FormFactor form_factor =
      variation_service_client_.GetCurrentFormFactor();
  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .form_factors = {form_factor},
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithDifferentFormFactor) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const Study::FormFactor current_form_factor =
      variation_service_client_.GetCurrentFormFactor();
  const Study::FormFactor all_form_factors[] = {
      Study::DESKTOP,
      Study::PHONE,
      Study::TABLET,
      Study::MEET_DEVICE,
  };
  for (const Study::FormFactor form_factor : all_form_factors) {
    if (form_factor == current_form_factor) {
      continue;
    }
    // The form factor don't match, so trial shouldn't be added.
    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        {.studies = {{.name = "TestTrial",
                      .experiments = {{.name = "TestGroup",
                                       .platforms = {platform},
                                       .form_factors = {form_factor},
                                       .params = {{"x", "1"}, {"y", "2"}}}}}}},
        Study::PLATFORM_ANDROID_WEBVIEW,
        variation_service_client_.GetCurrentFormFactor(), &feature_list);

    EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
    EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

    std::map<std::string, std::string> params;
    EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

    EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
  }
}

TEST_F(FieldTrialUtilTest, AssociateFeaturesFromFieldTrialConfig) {
  static BASE_FEATURE(kFeatureA, "A", base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureB, "B", base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureC, "C", base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureD, "D", base::FEATURE_ENABLED_BY_DEFAULT);

  const Study::Platform platform = Study::PLATFORM_LINUX;
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial1",
                    .experiments = {{.name = "TestGroup1",
                                     .platforms = {platform},
                                     .enable_features = {"A", "B"}}}},
                   {.name = "TestTrial2",
                    .experiments = {{.name = "TestGroup2",
                                     .platforms = {platform},
                                     .disable_features = {"C", "D"}},
                                    {.name = "TestGroup2-2",
                                     .platforms = {platform}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      feature_list.get());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Check the resulting feature and field trial states. Trials should not be
  // active until their associated features are queried.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestTrial1"));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureA));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureB));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestTrial1"));

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestTrial2"));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureC));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureD));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestTrial2"));
}

TEST_F(FieldTrialUtilTest, AssociateForcingFlagsFromFieldTrialConfig) {
  const Study::Platform platform = Study::PLATFORM_LINUX;
  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-2");
  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-3");

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial1",
             .experiments = {{.name = "TestGroup1", .platforms = {platform}}}},
            {.name = "TestTrial2",
             .experiments = {{.name = "TestGroup2", .platforms = {platform}},
                             {.name = "ForcedGroup2",
                              .platforms = {platform},
                              .forcing_flag = "flag-2"}}},
            {.name = "TestTrial3",
             .experiments = {{.name = "TestGroup3", .platforms = {platform}},
                             {.name = "ForcedGroup3",
                              .platforms = {platform},
                              .forcing_flag = "flag-3"},
                             {.name = "ForcedGroup3-2",
                              .platforms = {platform},
                              .forcing_flag = "flag-3-2"}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("ForcedGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
  EXPECT_EQ("ForcedGroup3", base::FieldTrialList::FindFullName("TestTrial3"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithIsLowEndDeviceMatch) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  // The is_low_end_device filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .is_low_end_device =
                                         base::SysInfo::IsLowEndDevice(),
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithIsLowEndDeviceMismatch) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  // The is_low_end_device don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .is_low_end_device =
                                         !base::SysInfo::IsLowEndDevice(),
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMinOsVersionMatch) {
  base::Version version = ClientFilterableState::GetOSVersion();
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  // The min_os_version filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .min_os_version =
                                         version.GetString().c_str(),
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithEnableBenchmarkingAndDisableBenchmarking) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;

  // When both enable-benchmarking and 'disable_benchmarking' are set,
  // the trial should not be added.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableBenchmarking);

  // The 'disable_benchmarking' field should prevent this trial from being
  // added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .disable_benchmarking = true,
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithEnableBenchmarkingAndNoDisableBenchmarking) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;

  // If enable-benchmarking is set, and the experiment doesn't set
  // 'disable_benchmarking', the trial should be added.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableBenchmarking);

  // The 'disable_benchmarking' field should prevent this trial from being
  // added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .disable_benchmarking = false,
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithEnableFieldTrialConfigBenchmarkingAndDisableBenchmarking) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;

  // When --enable-field-trial-config=benchmarking and 'disable_benchmarking'
  // are set, the trial should not be added.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableFieldTrialTestingConfig, "benchmarking");

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .disable_benchmarking = true,
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithEnableFieldTrialConfigBenchmarkingAndNoDisableBenchmarking) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;

  // If --enable-field-trial-config=benchmarking is set, and the experiment
  // doesn't set 'disable_benchmarking', the trial should be added.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableFieldTrialTestingConfig, "benchmarking");

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .disable_benchmarking = false,
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithNoEnableBenchmarkingWithDisableBenchmarking) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;

  // If there is no --enable-benchmarking switch, the trial should be added.
  // The 'disable_benchmarking' field out should prevent this trial from being
  // added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .disable_benchmarking = true,
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMinOsVersionMismatch) {
  base::Version version = ClientFilterableState::GetOSVersion();
  base::Version higher_version =
      base::Version({version.components()[0] + 1, 0, 0});
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  // The min_os_version doesn't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments = {{.name = "TestGroup",
                                     .platforms = {platform},
                                     .min_os_version =
                                         higher_version.GetString().c_str(),
                                     .params = {{"x", "1"}, {"y", "2"}}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest, TestEscapeValue) {
  std::string str = "trail.:/,*";
  std::string escaped_str = EscapeValue(str);
  EXPECT_EQ(escaped_str.find('.'), std::string::npos);
  EXPECT_EQ(escaped_str.find(':'), std::string::npos);
  EXPECT_EQ(escaped_str.find('/'), std::string::npos);
  EXPECT_EQ(escaped_str.find(','), std::string::npos);
  EXPECT_EQ(escaped_str.find('*'), std::string::npos);

  // Make sure the EscapeValue function is the inverse of base::UnescapeValue.
  EXPECT_EQ(str, base::UnescapeValue(escaped_str));
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareClassMatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial",
             .experiments =
                 {{.name = "TestGroup",
                   .platforms = {platform},
                   .hardware_classes = {hardware_class.c_str(),
                                        unmatched_hardware_class.c_str()}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareClassMismatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial",
             .experiments =
                 {{.name = "TestGroup",
                   .platforms = {platform},
                   .hardware_classes = {unmatched_hardware_class.c_str()}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithExcludHardwareClassMatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial",
             .experiments = {{.name = "TestGroup",
                              .platforms = {platform},
                              .exclude_hardware_classes =
                                  {hardware_class.c_str(),
                                   unmatched_hardware_class.c_str()}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithExcludHardwareClassMismatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies =
           {{.name = "TestTrial",
             .experiments = {{.name = "TestGroup",
                              .platforms = {platform},
                              .exclude_hardware_classes =
                                  {unmatched_hardware_class.c_str()}}}}}},
      platform, variation_service_client_.GetCurrentFormFactor(),
      &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareManufacturerMatch) {
  std::string hardware_manufacturer =
      ClientFilterableState::GetHardwareManufacturer();
  std::string unmatched_hardware_manufacturer = hardware_manufacturer + "foo";

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments =
                        {{.name = "TestGroup",
                          .platforms = {Study::PLATFORM_ANDROID,
                                        Study::PLATFORM_ANDROID_WEBVIEW},
                          .hardware_manufacturers =
                              {hardware_manufacturer.c_str(),
                               unmatched_hardware_manufacturer.c_str()}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareManufacturerMismatch) {
  std::string hardware_manufacturer =
      ClientFilterableState::GetHardwareManufacturer();
  std::string unmatched_hardware_manufacturer = hardware_manufacturer + "foo";

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments =
                        {{.name = "TestGroup",
                          .platforms = {Study::PLATFORM_ANDROID,
                                        Study::PLATFORM_ANDROID_WEBVIEW},
                          .hardware_manufacturers =
                              {unmatched_hardware_manufacturer.c_str()}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithExcludHardwareManufacturerMatch) {
  std::string hardware_manufacturer =
      ClientFilterableState::GetHardwareManufacturer();
  std::string unmatched_hardware_manufacturer = hardware_manufacturer + "foo";

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments =
                        {{.name = "TestGroup",
                          .platforms = {Study::PLATFORM_ANDROID,
                                        Study::PLATFORM_ANDROID_WEBVIEW},
                          .exclude_hardware_manufacturers =
                              {hardware_manufacturer.c_str(),
                               unmatched_hardware_manufacturer.c_str()}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(
    FieldTrialUtilTest,
    AssociateParamsFromFieldTrialConfigWithExcludHardwareManufacturerMismatch) {
  std::string hardware_manufacturer =
      ClientFilterableState::GetHardwareManufacturer();
  std::string unmatched_hardware_manufacturer = hardware_manufacturer + "foo";

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      {.studies = {{.name = "TestTrial",
                    .experiments =
                        {{.name = "TestGroup",
                          .platforms = {Study::PLATFORM_ANDROID,
                                        Study::PLATFORM_ANDROID_WEBVIEW},
                          .exclude_hardware_manufacturers =
                              {unmatched_hardware_manufacturer.c_str()}}}}}},
      Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace variations
