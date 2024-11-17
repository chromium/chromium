// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
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

namespace variations {
namespace {

class ExperimentBuilder {
 public:
  ExperimentBuilder() = default;

  FieldTrialTestingExperiment Build() const {
    return {
        name,
        platforms,
        form_factors,
        is_low_end_device,
        min_os_version,
        params,
        enable_features,
        disable_features,
        forcing_flag,
        override_ui_string,
        hardware_classes,
        exclude_hardware_classes,
    };
  }

  const char* name = nullptr;
  base::raw_span<const Study::Platform> platforms = {};
  base::raw_span<const Study::FormFactor> form_factors = {};
  std::optional<bool> is_low_end_device = std::nullopt;
  const char* min_os_version = nullptr;
  base::raw_span<const FieldTrialTestingExperimentParams> params = {};
  base::raw_span<const char*> enable_features = {};
  base::raw_span<const char*> disable_features = {};
  const char* forcing_flag = nullptr;
  base::raw_span<const OverrideUIString> override_ui_string = {};
  base::raw_span<const char*> hardware_classes = {};
  base::raw_span<const char*> exclude_hardware_classes = {};
};

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}

  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() = default;

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
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

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
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_0[] =
      {{"x", "1"}, {"y", "2"}};
  ExperimentBuilder study_0_experiment_0_builder;
  study_0_experiment_0_builder.name = "TestGroup1";
  study_0_experiment_0_builder.platforms = base::span_from_ref(platform);
  study_0_experiment_0_builder.params = array_kFieldTrialConfig_params_0;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      study_0_experiment_0_builder.Build(),
  };
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_1[] =
      {{"x", "3"}, {"y", "4"}};
  ExperimentBuilder study_1_experiment_0_builder;
  study_1_experiment_0_builder.name = "TestGroup2";
  study_1_experiment_0_builder.platforms = base::span_from_ref(platform);
  study_1_experiment_0_builder.params = array_kFieldTrialConfig_params_0;
  ExperimentBuilder study_1_experiment_1_builder;
  study_1_experiment_1_builder.name = "TestGroup2-2";
  study_1_experiment_1_builder.platforms = base::span_from_ref(platform);
  study_1_experiment_1_builder.params = array_kFieldTrialConfig_params_1;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      study_1_experiment_0_builder.Build(),
      study_1_experiment_1_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1},
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

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
  ExperimentBuilder experiment_0_builder;
  experiment_0_builder.name = "TestGroup0";
  experiment_0_builder.platforms = base::span_from_ref(platform);
  experiment_0_builder.enable_features = enable_features_0;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      experiment_0_builder.Build(),
  };

  const char* disable_features_1[] = {"B"};
  ExperimentBuilder experiment_1_builder;
  experiment_1_builder.name = "TestGroup1";
  experiment_1_builder.platforms = base::span_from_ref(platform);
  experiment_1_builder.disable_features = disable_features_1;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      experiment_1_builder.Build(),
  };

  const char* enable_features2[] = {"C"};
  const char* disable_features_2[] = {"D"};
  ExperimentBuilder experiment_2_builder;
  experiment_2_builder.name = "TestGroup2";
  experiment_2_builder.platforms = base::span_from_ref(platform);
  experiment_0_builder.enable_features = enable_features2;
  experiment_2_builder.disable_features = disable_features_2;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
      experiment_2_builder.Build(),
  };

  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {/*name=*/"TestTrial0",
       /*experiments=*/array_kFieldTrialConfig_experiments_0},
      {/*name=*/"TestTrial1",
       /*experiments=*/array_kFieldTrialConfig_experiments_1},
      {/*name=*/"TestTrial2",
       /*experiments=*/array_kFieldTrialConfig_experiments_2},
  };

  const FieldTrialTestingConfig kConfig = {
      /*studies=*/array_kFieldTrialConfig_studies};

  base::FeatureList feature_list;
  // Enable feature "A" and disable feature "B" as if they were enabled/disabled
  // using the |--enable-features| and |--disable-features| switches.
  base::FieldTrialList::CreateFieldTrial("Study", "Experiment");
  feature_list.InitFromCommandLine(/*enable_features=*/"A<Study",
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

  for (Study::Platform platform : all_platforms) {
    ExperimentBuilder experiment_builder;
    experiment_builder.name = "TestGroup";
    experiment_builder.platforms = base::span_from_ref(platform);
    experiment_builder.params = array_kFieldTrialConfig_params;
    const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
        experiment_builder.Build(),
    };
    const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
        {"TestTrial", array_kFieldTrialConfig_experiments}};
    const FieldTrialTestingConfig kConfig = {
        array_kFieldTrialConfig_studies,
    };

    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        kConfig, override_callback_.callback(), platform,
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
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithDifferentPlatform) {
  const Study::Platform platform = Study::PLATFORM_ANDROID;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // The platforms don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMultiplePlatforms) {
  const Study::Platform platforms[] =
      {Study::PLATFORM_ANDROID, Study::PLATFORM_ANDROID_WEBVIEW};
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = platforms;
  experiment_builder.params = array_kFieldTrialConfig_params;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // One of the platforms matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
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
  const Study::FormFactor form_factors[] = {
      Study::DESKTOP,
      Study::PHONE,
      Study::TABLET,
      Study::MEET_DEVICE,
      Study::FOLDABLE,
  };
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.form_factors = form_factors;
  experiment_builder.params = array_kFieldTrialConfig_params;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
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
       AssociateParamsFromFieldTrialConfigWithSingleFormFactor) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const Study::FormFactor form_factor =
      variation_service_client_.GetCurrentFormFactor();
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
        {{"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.form_factors = base::span_from_ref(form_factor);
  experiment_builder.params = array_kFieldTrialConfig_params;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
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
    ExperimentBuilder experiment_builder;
    experiment_builder.name = "TestGroup";
    experiment_builder.platforms = base::span_from_ref(platform);
    experiment_builder.form_factors = base::span_from_ref(form_factor);
    experiment_builder.params = array_kFieldTrialConfig_params;
    const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
        experiment_builder.Build(),
    };
    const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
        {"TestTrial", array_kFieldTrialConfig_experiments}};
    const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

    // The form factor don't match, so trial shouldn't be added.
    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(
        kConfig, override_callback_.callback(), Study::PLATFORM_ANDROID_WEBVIEW,
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

  const char* enable_features[] = {"A", "B"};
  const char* disable_features[] = {"C", "D"};

  const Study::Platform platform = Study::PLATFORM_LINUX;
  ExperimentBuilder study_0_experiment_0_builder;
  study_0_experiment_0_builder.name = "TestGroup1";
  study_0_experiment_0_builder.platforms = base::span_from_ref(platform);
  study_0_experiment_0_builder.enable_features = enable_features;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      study_0_experiment_0_builder.Build(),
  };
  ExperimentBuilder study_1_experiment_0_builder;
  study_1_experiment_0_builder.name = "TestGroup2";
  study_1_experiment_0_builder.platforms = base::span_from_ref(platform);
  study_1_experiment_0_builder.disable_features = disable_features;
  ExperimentBuilder study_1_experiment_1_builder;
  study_1_experiment_1_builder.name = "TestGroup2-2";
  study_1_experiment_1_builder.platforms = base::span_from_ref(platform);
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      study_1_experiment_0_builder.Build(),
      study_1_experiment_1_builder.Build(),
  };

  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1},
  };

  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

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
  ExperimentBuilder study_0_experiment_0_builder;
  study_0_experiment_0_builder.name = "TestGroup1";
  study_0_experiment_0_builder.platforms = base::span_from_ref(platform);
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      study_0_experiment_0_builder.Build(),
  };
  ExperimentBuilder study_1_experiment_0_builder;
  study_1_experiment_0_builder.name = "TestGroup2";
  study_1_experiment_0_builder.platforms = base::span_from_ref(platform);
  ExperimentBuilder study_1_experiment_1_builder;
  study_1_experiment_1_builder.name = "ForcedGroup2";
  study_1_experiment_1_builder.platforms = base::span_from_ref(platform);
  study_1_experiment_1_builder.forcing_flag = "flag-2";
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      study_1_experiment_0_builder.Build(),
      study_1_experiment_1_builder.Build(),
  };
  ExperimentBuilder study_2_experiment_0_builder;
  study_2_experiment_0_builder.name = "TestGroup3";
  study_2_experiment_0_builder.platforms = base::span_from_ref(platform);
  ExperimentBuilder study_2_experiment_1_builder;
  study_2_experiment_1_builder.name = "ForcedGroup3";
  study_2_experiment_1_builder.platforms = base::span_from_ref(platform);
  study_2_experiment_1_builder.forcing_flag = "flag-3";
  ExperimentBuilder study_2_experiment_2_builder;
  study_2_experiment_2_builder.name = "ForcedGroup3-2";
  study_2_experiment_2_builder.platforms = base::span_from_ref(platform);
  study_2_experiment_2_builder.forcing_flag = "flag-3-2";
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
      study_2_experiment_0_builder.Build(),
      study_2_experiment_1_builder.Build(),
      study_2_experiment_2_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1},
      {"TestTrial3", array_kFieldTrialConfig_experiments_2},
  };
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

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
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  experiment_builder.override_ui_string =
      array_kFieldTrialConfig_override_ui_string;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // One of the form_factors matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("1", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("2", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams("TestTrial", &params));
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
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  experiment_builder.is_low_end_device = base::SysInfo::IsLowEndDevice();
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // The is_low_end_device filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
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
       AssociateParamsFromFieldTrialConfigWithIsLowEndDeviceMismatch) {
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  experiment_builder.is_low_end_device = !base::SysInfo::IsLowEndDevice();
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // The is_low_end_device don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "x"));
  EXPECT_EQ("", base::GetFieldTrialParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(base::GetFieldTrialParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMinOsVersionMatch) {
  base::Version version = ClientFilterableState::GetOSVersion();
  std::string min_os_version = version.GetString();
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  experiment_builder.min_os_version = min_os_version.c_str();
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // The min_os_version filter matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
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
       AssociateParamsFromFieldTrialConfigWithMinOsVersionMismatch) {
  base::Version version = ClientFilterableState::GetOSVersion();
  base::Version higher_version =
      base::Version({version.components()[0] + 1, 0, 0});
  std::string min_os_version = higher_version.GetString();
  const Study::Platform platform = Study::PLATFORM_WINDOWS;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {"x", "1"}, {"y", "2"}};
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.params = array_kFieldTrialConfig_params;
  experiment_builder.min_os_version = min_os_version.c_str();
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      experiment_builder.Build(),
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial", array_kFieldTrialConfig_experiments}};
  const FieldTrialTestingConfig kConfig = {array_kFieldTrialConfig_studies};

  // The min_os_version doesn't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      kConfig, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareClassMatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";
  const char* classes[] = {
      hardware_class.c_str(),
      unmatched_hardware_class.c_str(),
  };

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.hardware_classes = classes;
  FieldTrialTestingExperiment experiment[]{experiment_builder.Build()};

  FieldTrialTestingStudy study[]{{
      /*name=*/"TestTrial",
      /*experiments=*/experiment,
  }};

  FieldTrialTestingConfig config = {
      /*studies=*/study,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      config, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithHardwareClassMismatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";
  const char* classes[] = {
      unmatched_hardware_class.c_str(),
  };

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.hardware_classes = classes;
  FieldTrialTestingExperiment experiment[]{experiment_builder.Build()};

  FieldTrialTestingStudy study[]{{
      /*name=*/"TestTrial",
      /*experiments=*/experiment,
  }};

  FieldTrialTestingConfig config = {
      /*studies=*/study,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      config, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithExcludHardwareClassMatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";
  const char* classes[] = {
      hardware_class.c_str(),
      unmatched_hardware_class.c_str(),
  };

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.exclude_hardware_classes = classes;
  FieldTrialTestingExperiment experiment[]{experiment_builder.Build()};

  FieldTrialTestingStudy study[]{{
      /*name=*/"TestTrial",
      /*experiments=*/experiment,
  }};

  FieldTrialTestingConfig config = {
      /*studies=*/study,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      config, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithExcludHardwareClassMismatch) {
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  std::string unmatched_hardware_class = hardware_class + "foo";
  const char* classes[] = {
      unmatched_hardware_class.c_str(),
  };

  Study::Platform platform = Study::PLATFORM_CHROMEOS;
  ExperimentBuilder experiment_builder;
  experiment_builder.name = "TestGroup";
  experiment_builder.platforms = base::span_from_ref(platform);
  experiment_builder.exclude_hardware_classes = classes;
  FieldTrialTestingExperiment experiment[]{experiment_builder.Build()};

  FieldTrialTestingStudy study[]{{
      /*name=*/"TestTrial",
      /*experiments=*/experiment,
  }};

  FieldTrialTestingConfig config = {
      /*studies=*/study,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(
      config, override_callback_.callback(), platform,
      variation_service_client_.GetCurrentFormFactor(), &feature_list);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

}  // namespace variations
