// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_feature_visitor.h"

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "early_boot_feature_visitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {

constexpr char kEarlyBootFeatureTestName[] = "CrOSEarlyBootTestFeature";
constexpr char kEarlyBootFeatureOffByDefaultName[] =
    "CrOSEarlyBootOffByDefault";
BASE_FEATURE(kEarlyBootFeatureOffByDefault,
             kEarlyBootFeatureOffByDefaultName,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

TEST(EarlyBootFeatureVisitor, FeatureWithNoFieldTrial) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEarlyBootFeatureOffByDefault);

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();

  ASSERT_EQ(overrides.size(), 1);
  const featured::FeatureOverride& feature = overrides[0];
  EXPECT_EQ(feature.name(), kEarlyBootFeatureOffByDefaultName);
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "");
  EXPECT_EQ(feature.group_name(), "");
  EXPECT_EQ(feature.params_size(), 0);
}

TEST(EarlyBootFeatureVisitor, InvalidFeatureName) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      "FeatureNameWithoutCrOSEarlyBootPrefix",
      base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE, trial);

  base::test::ScopedFeatureList initialized_field_feature_list;
  initialized_field_feature_list.InitWithFeatureList(std::move(feature_list));

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();

  ASSERT_EQ(overrides.size(), 0);
}

TEST(EarlyBootFeatureVisitor, FeatureOverrideUseDefault) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      kEarlyBootFeatureTestName, base::FeatureList::OVERRIDE_USE_DEFAULT,
      trial);

  base::test::ScopedFeatureList initialized_field_feature_list;
  initialized_field_feature_list.InitWithFeatureList(std::move(feature_list));

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();

  ASSERT_EQ(overrides.size(), 1);
  const featured::FeatureOverride& feature = overrides[0];
  EXPECT_EQ(feature.name(), kEarlyBootFeatureTestName);
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_USE_DEFAULT);
  EXPECT_EQ(feature.trial_name(), "TrialExample");
  EXPECT_EQ(feature.group_name(), "A");
}

TEST(EarlyBootFeatureVisitor, FeatureOverrideUseDisabled) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      kEarlyBootFeatureTestName, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      trial);

  base::test::ScopedFeatureList initialized_field_feature_list;
  initialized_field_feature_list.InitWithFeatureList(std::move(feature_list));

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();

  ASSERT_EQ(overrides.size(), 1);
  const featured::FeatureOverride& feature = overrides[0];
  EXPECT_EQ(feature.name(), kEarlyBootFeatureTestName);
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_DISABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "TrialExample");
  EXPECT_EQ(feature.group_name(), "A");
}

TEST(EarlyBootFeatureVisitor, FeatureOverrideUseEnabled) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      kEarlyBootFeatureTestName, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);

  base::test::ScopedFeatureList initialized_field_feature_list;
  initialized_field_feature_list.InitWithFeatureList(std::move(feature_list));

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();

  ASSERT_EQ(overrides.size(), 1);
  const featured::FeatureOverride& feature = overrides[0];
  EXPECT_EQ(feature.name(), kEarlyBootFeatureTestName);
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "TrialExample");
  EXPECT_EQ(feature.group_name(), "A");
}

TEST(EarlyBootFeatureVisitor, FeatureHasParams) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  base::test::ScopedFeatureList initialized_field_feature_list;

  initialized_field_feature_list.InitFromCommandLine(
      /*enabled_features=*/"CrOSEarlyBootTestFeature<foo.bar:k1/v1/k2/v2",
      /*disabled_features=*/"");

  EarlyBootFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);

  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      visitor.release_overrides();
  ASSERT_EQ(overrides.size(), 1);
  const featured::FeatureOverride& feature = overrides[0];
  EXPECT_EQ(feature.name(), kEarlyBootFeatureTestName);
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "foo");
  EXPECT_EQ(feature.group_name(), "bar");

  const std::map<std::string, std::string> expected_params = {{"k1", "v1"},
                                                              {"k2", "v2"}};
  std::map<std::string, std::string> actual_params;
  for (const auto& entry : feature.params()) {
    actual_params.insert({entry.key(), entry.value()});
  }
  EXPECT_EQ(actual_params, expected_params);
}

}  // namespace variations::cros_early_boot::evaluate_seed
