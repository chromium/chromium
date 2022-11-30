// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_seed_processor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations {
namespace {

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}

  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() {}

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const std::u16string& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;
};

// TODO(crbug.com/1167566): Remove when fake VariationsServiceClient created.
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
  FieldTrialUtilTest() {}

  FieldTrialUtilTest(const FieldTrialUtilTest&) = delete;
  FieldTrialUtilTest& operator=(const FieldTrialUtilTest&) = delete;

  ~FieldTrialUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

 protected:
  TestOverrideStringCallback override_callback_;
  TestVariationsServiceClient variation_service_client_;
};

}  // namespace

TEST_F(FieldTrialUtilTest, AssociateParamsFromString) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.B:a/%2F";
  ASSERT_TRUE(AssociateParamsFromString(kVariationsString));

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("/", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams(kTrialName, &params));
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
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_0[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       array_kFieldTrialConfig_params_0,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_1[] =
      {{"x", "3"}, {"y", "4"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       array_kFieldTrialConfig_params_0,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
      {"TestGroup2-2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       array_kFieldTrialConfig_params_1,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 2
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial1", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial1", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial1", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("TestGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
}

// Verifies that studies in the field trial config should be skipped if they
// enable/disable features that were overridden through the command line.
TEST_F(FieldTrialUtilTest, FieldTrialConfigSkipOverridden) {
  // Create a testing config equivalent to:
  // {
  //   "TestTrial0": [
  //       {
  //           "platforms": ["linux"],
  //           "experiments": [
  //               {
  //                   "name": "TestGroup0",
  //                   "enable_features": ["A"]
  //               }
  //           ]
  //       }
  //   ],
  //   "TestTrial1": [
  //       {
  //           "platforms": ["linux"],
  //           "experiments": [
  //               {
  //                   "name": "TestGroup1",
  //                   "disable_features": ["A"]
  //               }
  //           ]
  //       }
  //   ],
  //   "TestTrial2": [
  //       {
  //           "platforms": ["linux"],
  //           "experiments": [
  //               {
  //                   "name": "TestGroup2",
  //                   "enable_features": ["C"],
  //                   "disable_features": ["D"]
  //               }
  //           ]
  //       }
  //   ]
  // }
  const Study::Platform platform = Study::PLATFORM_LINUX;

  const char* enable_features_0[] = {"A"};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {/*name=*/"TestGroup0",
       /*platforms=*/&platform,
       /*platforms_size=*/1,
       /*form_factors=*/{},
       /*form_factors_size=*/0,
       /*is_low_end_device=*/absl::nullopt,
       /*min_os_version=*/nullptr,
       /*params=*/nullptr,
       /*params_size=*/0,
       /*enable_features=*/enable_features_0,
       /*enable_features_size=*/1,
       /*disable_features=*/nullptr,
       /*disable_features_size=*/0,
       /*forcing_flag=*/nullptr,
       /*override_ui_string=*/nullptr,
       /*override_ui_string_size=*/0},
  };

  const char* disable_features_1[] = {"B"};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {/*name=*/"TestGroup1",
       /*platforms=*/&platform,
       /*platforms_size=*/1,
       /*form_factors=*/{},
       /*form_factors_size=*/0,
       /*is_low_end_device=*/absl::nullopt,
       /*min_os_version=*/nullptr,
       /*params=*/nullptr,
       /*params_size=*/0,
       /*enable_features=*/nullptr,
       /*enable_features_size=*/0,
       /*disable_features=*/disable_features_1,
       /*disable_features_size=*/1,
       /*forcing_flag=*/nullptr,
       /*override_ui_string=*/nullptr,
       /*override_ui_string_size=*/0},
  };

  const char* enable_features2[] = {"C"};
  const char* disable_features_2[] = {"D"};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
      {/*name=*/"TestGroup2",
       /*platforms=*/&platform,
       /*platforms_size=*/1,
       /*form_factors=*/{},
       /*form_factors_size=*/0,
       /*is_low_end_device=*/absl::nullopt,
       /*min_os_version=*/nullptr,
       /*params=*/nullptr,
       /*params_size=*/0,
       /*enable_features=*/enable_features2,
       /*enable_features_size=*/1,
       /*disable_features=*/disable_features_2,
       /*disable_features_size=*/1,
       /*forcing_flag=*/nullptr,
       /*override_ui_string=*/nullptr,
       /*override_ui_string_size=*/0},
  };

  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {/*name=*/"TestTrial0",
       /*experiments=*/array_kFieldTrialConfig_experiments_0,
       /*experiments_size=*/1},
      {/*name=*/"TestTrial1",
       /*experiments=*/array_kFieldTrialConfig_experiments_1,
       /*experiments_size=*/1},
      {/*name=*/"TestTrial2",
       /*experiments=*/array_kFieldTrialConfig_experiments_2,
       /*experiments_size=*/1},
  };

  const FieldTrialTestingConfig kConfig = {
      /*studies=*/array_kFieldTrialConfig_studies, /*studies_size=*/3};

  base::FeatureList feature_list;
  // Enable feature "A" and disable feature "B" as if they were enabled/disabled
  // using the |--enable-features| and |--disable-features| switches.
  base::FieldTrialList::CreateFieldTrial("Study", "Experiment");
  feature_list.InitializeFromCommandLine(/*enable_features=*/"A<Study",
                                         /*disable_features=*/"B");

  // Associate the |kConfig| field trial config.
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

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
      Study::PLATFORM_ANDROID_WEBLAYER,
      Study::PLATFORM_ANDROID_WEBVIEW,
      Study::PLATFORM_CHROMEOS,
      Study::PLATFORM_CHROMEOS_LACROS,
      Study::PLATFORM_FUCHSIA,
      Study::PLATFORM_IOS,
      Study::PLATFORM_LINUX,
      Study::PLATFORM_MAC,
      Study::PLATFORM_WINDOWS,
  };

  // Break if platforms are added without updating |all_platforms|.
  static_assert(std::size(all_platforms) == Study::Platform_ARRAYSIZE,
                "|all_platforms| must include all platforms.");

  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};

  for (size_t i = 0; i < std::size(all_platforms); ++i) {
    const Study::Platform platform = all_platforms[i];
    const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
        {"TestGroup",
         &platform,
         1,
         {},
         0,
         absl::nullopt,
         nullptr,
         array_kFieldTrialConfig_params,
         2,
         nullptr,
         0,
         nullptr,
         0,
         nullptr,
         nullptr,
         0},
    };
    const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
        {"TestTrial", array_kFieldTrialConfig_experiments, 1}
    };
    const FieldTrialTestingConfig kConfig = {
        array_kFieldTrialConfig_studies, 1
    };

    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        kConfig, override_callback_.callback(), platform,
        variation_service_client_.GetCurrentFormFactor(), &feature_list);

    EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
    EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

    std::map<std::string, std::string> params;
    EXPECT_TRUE(GetVariationParams("TestTrial", &params));
    EXPECT_EQ(2U, params.size());
    EXPECT_EQ("1", params["x"]);
    EXPECT_EQ("2", params["y"]);

    EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
  }
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithDifferentPlatform) {
  const Study::Platform platform = Study::PLATFORM_ANDROID;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
      {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig =
      {array_kFieldTrialConfig_studies, 1};

  // The platforms don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMultiplePlatforms) {
  const Study::Platform platforms[] =
      {Study::PLATFORM_ANDROID, Study::PLATFORM_ANDROID_WEBVIEW};
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       platforms,
       2,
       {},
       0,
       absl::nullopt,
       nullptr,
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
      {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig =
      {array_kFieldTrialConfig_studies, 1};

  // One of the platforms matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithAllFormFactors) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const Study::FormFactor form_factors[] = {
      Study::DESKTOP,
      Study::PHONE,
      Study::TABLET,
      Study::MEET_DEVICE,
  };
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup", &platform, 1, form_factors, 4, absl::nullopt, nullptr,
       array_kFieldTrialConfig_params, 2, nullptr, 0, nullptr, 0, nullptr,
       nullptr, 0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
      {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig =
      {array_kFieldTrialConfig_studies, 1};

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
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
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
        {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup", &platform, 1, &form_factor, 1, absl::nullopt, nullptr,
       array_kFieldTrialConfig_params, 2, nullptr, 0, nullptr, 0, nullptr,
       nullptr, 0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 1
  };

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
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
    if (form_factor == current_form_factor)
      continue;
    const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
        {{"x", "1"}, {"y", "2"}};
    const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
        {"TestGroup", &platform, 1, &form_factor, 1, absl::nullopt, nullptr,
         array_kFieldTrialConfig_params, 2, nullptr, 0, nullptr, 0, nullptr,
         nullptr, 0},
    };
    const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
        {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
    const FieldTrialTestingConfig kConfig =
        {array_kFieldTrialConfig_studies, 1};

    // The form factor don't match, so trial shouldn't be added.
    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
        variation_service_client_.GetCurrentFormFactor(), &feature_list);

    EXPECT_EQ("", GetVariationParamValue("TestTrial", "x"));
    EXPECT_EQ("", GetVariationParamValue("TestTrial", "y"));

    std::map<std::string, std::string> params;
    EXPECT_FALSE(GetVariationParams("TestTrial", &params));

    EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
  }
}

TEST_F(FieldTrialUtilTest, AssociateFeaturesFromFieldTrialConfig) {
  static BASE_FEATURE(kFeatureA, "A", base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureB, "B", base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureC, "C", base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureD, "D", base::FEATURE_ENABLED_BY_DEFAULT);

  const char* enable_features[] = {"A", "B"};
  const char* disable_features[] = {"C", "D"};

  const Study::Platform platform = Study::PLATFORM_LINUX;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       enable_features,
       2,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       disable_features,
       2,
       nullptr,
       nullptr,
       0},
      {"TestGroup2-2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
  };

  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
  };

  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 2
  };

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), feature_list.get());
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
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
      {"ForcedGroup2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       "flag-2",
       nullptr,
       0},
  };
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
      {"TestGroup3",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       nullptr,
       0},
      {"ForcedGroup3",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       "flag-3",
       nullptr,
       0},
      {"ForcedGroup3-2",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       nullptr,
       nullptr,
       0,
       nullptr,
       0,
       nullptr,
       0,
       "flag-3-2",
       nullptr,
       0},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
      {"TestTrial3", array_kFieldTrialConfig_experiments_2, 3},
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 3
  };

  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-2");
  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-3");

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("ForcedGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
  EXPECT_EQ("ForcedGroup3", base::FieldTrialList::FindFullName("TestTrial3"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithUIStringOverrides) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
        {{"x", "1"}, {"y", "2"}};
  const OverrideUIString array_kFieldTrialConfig_override_ui_string[] =
        {{1234, "test1"}, {5678, "test2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup", &platform, 1, nullptr, 0, absl::nullopt, nullptr,
       array_kFieldTrialConfig_params, 2, nullptr, 0, nullptr, 0, nullptr,
       array_kFieldTrialConfig_override_ui_string, 2},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 1
  };

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
  const TestOverrideStringCallback::OverrideMap& overrides =
      override_callback_.overrides();
  EXPECT_EQ(2u, overrides.size());
  auto it = overrides.find(1234);
  EXPECT_EQ(u"test1", it->second);
  it = overrides.find(5678);
  EXPECT_EQ(u"test2", it->second);
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithIsLowEndDeviceMatch) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       &platform,
       1,
       {},
       0,
       base::SysInfo::IsLowEndDevice(),
       nullptr,
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies, 1};

  // The is_low_end_device filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithIsLowEndDeviceMismatch) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       &platform,
       1,
       {},
       0,
       !base::SysInfo::IsLowEndDevice(),
       nullptr,
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies, 1};

  // The is_low_end_device don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMinOsVersionMatch) {
  base::Version version = ClientFilterableState::GetOSVersion();
  std::string min_os_version = version.GetString();
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       min_os_version.c_str(),
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies, 1};

  // The min_os_version filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
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
  std::string min_os_version = higher_version.GetString();
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup",
       &platform,
       1,
       {},
       0,
       absl::nullopt,
       min_os_version.c_str(),
       array_kFieldTrialConfig_params,
       2,
       nullptr,
       0,
       nullptr,
       0,
       nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies, 1};

  // The min_os_version doesn't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams("TestTrial", &params));

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
}  // namespace variations
