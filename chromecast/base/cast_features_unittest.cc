// Copyright 2017 The Chromium Authors. All rights reserved.
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
  base::Feature bool_feature{kTestBooleanFeatureName,
                             base::FEATURE_DISABLED_BY_DEFAULT};
  base::Feature bool_feature_2{kTestBooleanFeatureName2,
                               base::FEATURE_ENABLED_BY_DEFAULT};
  base::Feature bool_feature_3{kTestBooleanFeatureName3,
                               base::FEATURE_DISABLED_BY_DEFAULT};
  base::Feature bool_feature_4{kTestBooleanFeatureName4,
                               base::FEATURE_ENABLED_BY_DEFAULT};

  // Properly register them
  chromecast::SetFeaturesForTest(
      {&bool_feature, &bool_feature_2, &bool_feature_3, &bool_feature_4});

  // Override those features with DCS configs.
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);
  features.SetBoolKey(kTestBooleanFeatureName, false);
  features.SetBoolKey(kTestBooleanFeatureName2, false);
  features.SetBoolKey(kTestBooleanFeatureName3, true);
  features.SetBoolKey(kTestBooleanFeatureName4, true);

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
  base::Feature test_feature{kTestParamsFeatureName,
                             base::FEATURE_DISABLED_BY_DEFAULT};
  chromecast::SetFeaturesForTest({&test_feature});

  // Pass params via DCS.
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetStringKey("foo_key", "foo");
  params.SetStringKey("bar_key", "bar");
  params.SetStringKey("doub_key", "3.14159");
  params.SetStringKey("long_doub_key", "1.23459999999999999");
  params.SetStringKey("int_key", "4242");
  params.SetStringKey("bool_key", "true");
  features.SetPath(kTestParamsFeatureName, std::move(params));

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
  base::Feature bool_feature{kTestBooleanFeatureName,
                             base::FEATURE_DISABLED_BY_DEFAULT};
  base::Feature bool_feature_2{kTestBooleanFeatureName2,
                               base::FEATURE_ENABLED_BY_DEFAULT};
  base::Feature bool_feature_3{kTestBooleanFeatureName3,
                               base::FEATURE_DISABLED_BY_DEFAULT};
  base::Feature bool_feature_4{kTestBooleanFeatureName4,
                               base::FEATURE_ENABLED_BY_DEFAULT};

  // Override those features with DCS configs.
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);
  features.SetBoolKey(kTestBooleanFeatureName, false);
  features.SetBoolKey(kTestBooleanFeatureName2, false);
  features.SetBoolKey(kTestBooleanFeatureName3, true);
  features.SetBoolKey(kTestBooleanFeatureName4, true);

  // Also override a param feature with DCS config.
  base::Feature params_feature{kTestParamsFeatureName,
                               base::FEATURE_ENABLED_BY_DEFAULT};
  chromecast::SetFeaturesForTest({&bool_feature, &bool_feature_2,
                                  &bool_feature_3, &bool_feature_4,
                                  &params_feature});

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetStringKey("foo_key", "foo");
  features.SetPath(kTestParamsFeatureName, std::move(params));

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
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(0u, GetDCSExperimentIds().size());
}

TEST_F(CastFeaturesTest, SetGoodExperiments) {
  // Override those features with DCS configs.
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);

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
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);
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
  base::Value experiments(base::Value::Type::LIST);
  base::Value features(base::Value::Type::DICTIONARY);
  experiments.Append("foobar");
  experiments.Append(true);
  experiments.Append(1.23456);

  std::unordered_set<int32_t> expected;

  InitializeFeatureList(features, experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage) {
  base::Value features(base::Value::Type::DICTIONARY);
  features.SetBoolKey("bool_key", false);
  features.SetBoolKey("bool_key_2", true);

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetStringKey("foo_key", "foo");
  params.SetStringKey("bar_key", "bar");
  params.SetDoubleKey("doub_key", 3.14159);
  params.SetDoubleKey("long_doub_key", 1.234599999999999);
  params.SetIntKey("int_key", 4242);
  params.SetIntKey("negint_key", -273);
  params.SetBoolKey("bool_key", true);
  features.SetPath("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(features);
  ASSERT_EQ(3u, dict.DictSize());
  auto bval = dict.FindBoolKey("bool_key");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(false, *bval);
  bval = dict.FindBoolKey("bool_key_2");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(true, *bval);

  const auto* dval = dict.FindDictKey("params_key");
  const std::string* sval = nullptr;
  ASSERT_TRUE(dval);
  ASSERT_EQ(7u, dval->DictSize());
  sval = dval->FindStringKey("foo_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("foo", *sval);
  sval = dval->FindStringKey("bar_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("bar", *sval);
  sval = dval->FindStringKey("doub_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("3.14159", *sval);
  sval = dval->FindStringKey("long_doub_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("1.234599999999999", *sval);
  sval = dval->FindStringKey("int_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("4242", *sval);
  sval = dval->FindStringKey("negint_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("-273", *sval);
  sval = dval->FindStringKey("bool_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("true", *sval);
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage_BadParams) {
  base::Value features(base::Value::Type::DICTIONARY);
  features.SetBoolKey("bool_key", false);
  features.SetStringKey("str_key", "foobar");
  features.SetIntKey("int_key", 12345);
  features.SetDoubleKey("doub_key", 4.5678);

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetStringKey("foo_key", "foo");
  features.SetPath("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(features);
  ASSERT_EQ(2u, dict.DictSize());
  auto bval = dict.FindBoolKey("bool_key");
  ASSERT_TRUE(bval.has_value());
  ASSERT_EQ(false, *bval);

  const auto* dval = dict.FindDictKey("params_key");
  ASSERT_TRUE(dval);
  ASSERT_EQ(1u, dval->DictSize());
  const auto* sval = dval->FindStringKey("foo_key");
  ASSERT_TRUE(sval);
  ASSERT_EQ("foo", *sval);
}

}  // namespace chromecast
