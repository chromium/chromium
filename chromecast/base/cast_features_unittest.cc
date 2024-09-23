// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kTestBooleanFeatureName[] = "test_boolean_feature";
const char kTestBooleanFeatureName2[] = "test_boolean_feature_2";
const char kTestBooleanFeatureName3[] = "test_boolean_feature_3";
const char kTestBooleanFeatureName4[] = "test_boolean_feature_4";

const char kTestParamsFeatureName[] = "test_params_feature";

}  // namespace

class CastFeaturesTest : public testing::Test {
 public:
  CastFeaturesTest() {}

  CastFeaturesTest(const CastFeaturesTest&) = delete;
  CastFeaturesTest& operator=(const CastFeaturesTest&) = delete;

  ~CastFeaturesTest() override {}

  // testing::Test implementation:
  void SetUp() override {
    original_feature_list_ = base::FeatureList::ClearInstanceForTesting();
    ResetCastFeaturesForTesting();
  }
  void TearDown() override {
    ResetCastFeaturesForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(original_feature_list_));
  }

 private:
  std::unique_ptr<base::FeatureList> original_feature_list_;
};

TEST_F(CastFeaturesTest, EnableDisableMultipleBooleanFeatures) {
  // Declare several boolean features.
  static BASE_FEATURE(bool_feature, kTestBooleanFeatureName,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_2, kTestBooleanFeatureName2,
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_3, kTestBooleanFeatureName3,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_4, kTestBooleanFeatureName4,
                      base::FEATURE_ENABLED_BY_DEFAULT);

  // Properly register them
  chromecast::SetFeaturesForTest(
      {&bool_feature, &bool_feature_2, &bool_feature_3, &bool_feature_4});

  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;
  features.Set(kTestBooleanFeatureName, false);
  features.Set(kTestBooleanFeatureName2, false);
  features.Set(kTestBooleanFeatureName3, true);
  features.Set(kTestBooleanFeatureName4, true);

  InitializeFeatureList(features, experiments, "", "", "", "");

  // Test that features are properly enabled (they should match the
  // DCS config).
  ASSERT_FALSE(chromecast::IsFeatureEnabled(bool_feature));
  ASSERT_FALSE(chromecast::IsFeatureEnabled(bool_feature_2));
  ASSERT_TRUE(chromecast::IsFeatureEnabled(bool_feature_3));
  ASSERT_TRUE(chromecast::IsFeatureEnabled(bool_feature_4));
}

TEST_F(CastFeaturesTest, EnableSingleFeatureWithParams) {
  // Define a feature with params.
  static BASE_FEATURE(test_feature, kTestParamsFeatureName,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  chromecast::SetFeaturesForTest({&test_feature});

  // Pass params via DCS.
  base::Value::List experiments;
  base::Value::Dict features;
  base::Value::Dict params;
  params.Set("foo_key", "foo");
  params.Set("bar_key", "bar");
  params.Set("doub_key", "3.14159");
  params.Set("long_doub_key", "1.23459999999999999");
  params.Set("int_key", "4242");
  params.Set("bool_key", "true");
  features.Set(kTestParamsFeatureName, std::move(params));

  InitializeFeatureList(features, experiments, "", "", "", "");

  // Test that this feature is enabled, and params are correct.
  ASSERT_TRUE(chromecast::IsFeatureEnabled(test_feature));
  ASSERT_EQ("foo",
            base::GetFieldTrialParamValueByFeature(test_feature, "foo_key"));
  ASSERT_EQ("bar",
            base::GetFieldTrialParamValueByFeature(test_feature, "bar_key"));
  ASSERT_EQ(3.14159, base::GetFieldTrialParamByFeatureAsDouble(
                         test_feature, "doub_key", 0.000));
  ASSERT_EQ(1.23459999999999999, base::GetFieldTrialParamByFeatureAsDouble(
                                     test_feature, "long_doub_key", 0.000));
  ASSERT_EQ(4242, base::GetFieldTrialParamByFeatureAsInt(test_feature,
                                                         "int_key", -1));
  ASSERT_EQ(true, base::GetFieldTrialParamByFeatureAsBool(test_feature,
                                                          "bool_key", false));
}

TEST_F(CastFeaturesTest, CommandLineOverridesDcsAndDefault) {
  // Declare several boolean features.
  static BASE_FEATURE(bool_feature, kTestBooleanFeatureName,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_2, kTestBooleanFeatureName2,
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_3, kTestBooleanFeatureName3,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(bool_feature_4, kTestBooleanFeatureName4,
                      base::FEATURE_ENABLED_BY_DEFAULT);

  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;
  features.Set(kTestBooleanFeatureName, false);
  features.Set(kTestBooleanFeatureName2, false);
  features.Set(kTestBooleanFeatureName3, true);
  features.Set(kTestBooleanFeatureName4, true);

  // Also override a param feature with DCS config.
  static BASE_FEATURE(params_feature, kTestParamsFeatureName,
                      base::FEATURE_ENABLED_BY_DEFAULT);
  chromecast::SetFeaturesForTest({&bool_feature, &bool_feature_2,
                                  &bool_feature_3, &bool_feature_4,
                                  &params_feature});

  base::Value::Dict params;
  params.Set("foo_key", "foo");
  features.Set(kTestParamsFeatureName, std::move(params));

  // Now override with command line flags. Command line flags should have the
  // final say.
  std::string enabled_features = std::string(kTestBooleanFeatureName)
                                     .append(",")
                                     .append(kTestBooleanFeatureName2);

  std::string disabled_features = std::string(kTestBooleanFeatureName4)
                                      .append(",")
                                      .append(kTestParamsFeatureName);

  InitializeFeatureList(features, experiments, enabled_features,
                        disabled_features, "", "");

  // Test that features are properly enabled (they should match the
  // DCS config).
  ASSERT_TRUE(chromecast::IsFeatureEnabled(bool_feature));
  ASSERT_TRUE(chromecast::IsFeatureEnabled(bool_feature_2));
  ASSERT_TRUE(chromecast::IsFeatureEnabled(bool_feature_3));
  ASSERT_FALSE(chromecast::IsFeatureEnabled(bool_feature_4));

  // Test that the params feature is disabled, and params are not set.
  ASSERT_FALSE(chromecast::IsFeatureEnabled(params_feature));
  ASSERT_EQ("",
            base::GetFieldTrialParamValueByFeature(params_feature, "foo_key"));
}

TEST_F(CastFeaturesTest, SetEmptyExperiments) {
  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(0u, GetDCSExperimentIds().size());
}

TEST_F(CastFeaturesTest, SetGoodExperiments) {
  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;

  int32_t ids[] = {12345678, 123, 0, -1};
  std::unordered_set<int32_t> expected;
  for (int32_t id : ids) {
    experiments.Append(id);
    expected.insert(id);
  }

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, SetSomeGoodExperiments) {
  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;
  experiments.Append(1234);
  experiments.Append("foobar");
  experiments.Append(true);
  experiments.Append(1);
  experiments.Append(1.23456);

  std::unordered_set<int32_t> expected;
  expected.insert(1234);
  expected.insert(1);

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, SetAllBadExperiments) {
  // Override those features with DCS configs.
  base::Value::List experiments;
  base::Value::Dict features;
  experiments.Append("foobar");
  experiments.Append(true);
  experiments.Append(1.23456);

  std::unordered_set<int32_t> expected;

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage) {
  base::Value::Dict features;
  features.Set("bool_key", false);
  features.Set("bool_key_2", true);

  base::Value::Dict params;
  params.Set("foo_key", "foo");
  params.Set("bar_key", "bar");
  params.Set("doub_key", 3.14159);
  params.Set("long_doub_key", 1.234599999999999);
  params.Set("int_key", 4242);
  params.Set("negint_key", -273);
  params.Set("bool_key", true);
  features.Set("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(features);
  ASSERT_EQ(3u, dict.size());
  auto bval = dict.FindBool("bool_key");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(false, *bval);
  bval = dict.FindBool("bool_key_2");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(true, *bval);

  const auto* dval = dict.FindDict("params_key");
  const std::string* sval = nullptr;
  ASSERT_TRUE(dval);
  ASSERT_EQ(7u, dval->size());
  sval = dval->FindString("foo_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("foo", *sval);
  sval = dval->FindString("bar_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("bar", *sval);
  sval = dval->FindString("doub_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("3.14159", *sval);
  sval = dval->FindString("long_doub_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("1.234599999999999", *sval);
  sval = dval->FindString("int_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("4242", *sval);
  sval = dval->FindString("negint_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("-273", *sval);
  sval = dval->FindString("bool_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("true", *sval);
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage_BadParams) {
  base::Value::Dict features;
  features.Set("bool_key", false);
  features.Set("str_key", "foobar");
  features.Set("int_key", 12345);
  features.Set("doub_key", 4.5678);

  base::Value::Dict params;
  params.Set("foo_key", "foo");
  features.Set("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(features);
  ASSERT_EQ(2u, dict.size());
  auto bval = dict.FindBool("bool_key");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(false, *bval);

  const auto* dval = dict.FindDict("params_key");
  ASSERT_TRUE(dval);
  ASSERT_EQ(1u, dval->size());
  const auto* sval = dval->FindString("foo_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("foo", *sval);
}

}  // namespace chromecast
