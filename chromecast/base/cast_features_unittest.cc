// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"

#include "base/macros.h"
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

  DISALLOW_COPY_AND_ASSIGN(CastFeaturesTest);
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
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();
  features->SetBoolean(kTestBooleanFeatureName, false);
  features->SetBoolean(kTestBooleanFeatureName2, false);
  features->SetBoolean(kTestBooleanFeatureName3, true);
  features->SetBoolean(kTestBooleanFeatureName4, true);

  InitializeFeatureList(*features, *experiments, "", "", "", "");

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
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();
  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("foo_key", "foo");
  params->SetString("bar_key", "bar");
  params->SetString("doub_key", "3.14159");
  params->SetString("long_doub_key", "1.23459999999999999");
  params->SetString("int_key", "4242");
  params->SetString("bool_key", "true");
  features->Set(kTestParamsFeatureName, std::move(params));

  InitializeFeatureList(*features, *experiments, "", "", "", "");

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
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();
  features->SetBoolean(kTestBooleanFeatureName, false);
  features->SetBoolean(kTestBooleanFeatureName2, false);
  features->SetBoolean(kTestBooleanFeatureName3, true);
  features->SetBoolean(kTestBooleanFeatureName4, true);

  // Also override a param feature with DCS config.
  base::Feature params_feature{kTestParamsFeatureName,
                               base::FEATURE_ENABLED_BY_DEFAULT};
  chromecast::SetFeaturesForTest({&bool_feature, &bool_feature_2,
                                  &bool_feature_3, &bool_feature_4,
                                  &params_feature});

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("foo_key", "foo");
  features->Set(kTestParamsFeatureName, std::move(params));

  // Now override with command line flags. Command line flags should have the
  // final say.
  std::string enabled_features = std::string(kTestBooleanFeatureName)
                                     .append(",")
                                     .append(kTestBooleanFeatureName2);

  std::string disabled_features = std::string(kTestBooleanFeatureName4)
                                      .append(",")
                                      .append(kTestParamsFeatureName);

  InitializeFeatureList(*features, *experiments, enabled_features,
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
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();

  InitializeFeatureList(*features, *experiments, "", "", "", "");
  ASSERT_EQ(0u, GetDCSExperimentIds().size());
}

TEST_F(CastFeaturesTest, SetGoodExperiments) {
  // Override those features with DCS configs.
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();

  int32_t ids[] = {12345678, 123, 0, -1};
  std::unordered_set<int32_t> expected;
  for (int32_t id : ids) {
    experiments->AppendInteger(id);
    expected.insert(id);
  }

  InitializeFeatureList(*features, *experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, SetSomeGoodExperiments) {
  // Override those features with DCS configs.
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();
  experiments->AppendInteger(1234);
  experiments->AppendString("foobar");
  experiments->AppendBoolean(true);
  experiments->AppendInteger(1);
  experiments->AppendDouble(1.23456);

  std::unordered_set<int32_t> expected;
  expected.insert(1234);
  expected.insert(1);

  InitializeFeatureList(*features, *experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, SetAllBadExperiments) {
  // Override those features with DCS configs.
  auto experiments = std::make_unique<base::ListValue>();
  auto features = std::make_unique<base::DictionaryValue>();
  experiments->AppendString("foobar");
  experiments->AppendBoolean(true);
  experiments->AppendDouble(1.23456);

  std::unordered_set<int32_t> expected;

  InitializeFeatureList(*features, *experiments, "", "", "", "");
  ASSERT_EQ(expected, GetDCSExperimentIds());
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage) {
  auto features = std::make_unique<base::DictionaryValue>();
  features->SetBoolean("bool_key", false);
  features->SetBoolean("bool_key_2", true);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("foo_key", "foo");
  params->SetString("bar_key", "bar");
  params->SetDouble("doub_key", 3.14159);
  params->SetDouble("long_doub_key", 1.234599999999999);
  params->SetInteger("int_key", 4242);
  params->SetInteger("negint_key", -273);
  params->SetBoolean("bool_key", true);
  features->Set("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(*features);
  bool bval;
  ASSERT_EQ(3u, dict.size());
  ASSERT_TRUE(dict.GetBoolean("bool_key", &bval));
  ASSERT_EQ(false, bval);
  ASSERT_TRUE(dict.GetBoolean("bool_key_2", &bval));
  ASSERT_EQ(true, bval);

  const base::DictionaryValue* dval;
  std::string sval;
  ASSERT_TRUE(dict.GetDictionary("params_key", &dval));
  ASSERT_EQ(7u, dval->size());
  ASSERT_TRUE(dval->GetString("foo_key", &sval));
  ASSERT_EQ("foo", sval);
  ASSERT_TRUE(dval->GetString("bar_key", &sval));
  ASSERT_EQ("bar", sval);
  ASSERT_TRUE(dval->GetString("doub_key", &sval));
  ASSERT_EQ("3.14159", sval);
  ASSERT_TRUE(dval->GetString("long_doub_key", &sval));
  ASSERT_EQ("1.234599999999999", sval);
  ASSERT_TRUE(dval->GetString("int_key", &sval));
  ASSERT_EQ("4242", sval);
  ASSERT_TRUE(dval->GetString("negint_key", &sval));
  ASSERT_EQ("-273", sval);
  ASSERT_TRUE(dval->GetString("bool_key", &sval));
  ASSERT_EQ("true", sval);
}

TEST_F(CastFeaturesTest, GetOverriddenFeaturesForStorage_BadParams) {
  auto features = std::make_unique<base::DictionaryValue>();
  features->SetBoolean("bool_key", false);
  features->SetString("str_key", "foobar");
  features->SetInteger("int_key", 12345);
  features->SetDouble("doub_key", 4.5678);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("foo_key", "foo");
  features->Set("params_key", std::move(params));

  auto dict = GetOverriddenFeaturesForStorage(*features);
  bool bval;
  ASSERT_EQ(2u, dict.size());
  ASSERT_TRUE(dict.GetBoolean("bool_key", &bval));
  ASSERT_EQ(false, bval);

  const base::DictionaryValue* dval;
  std::string sval;
  ASSERT_TRUE(dict.GetDictionary("params_key", &dval));
  ASSERT_EQ(1u, dval->size());
  ASSERT_TRUE(dval->GetString("foo_key", &sval));
  ASSERT_EQ("foo", sval);
}

}  // namespace chromecast
