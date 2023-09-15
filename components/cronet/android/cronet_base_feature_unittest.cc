// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_base_feature.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {
namespace {

using ::org::chromium::net::httpflags::BaseFeatureOverrides;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// base::Feature uses a caching mechanism where repeated
// `base::FeatureList::IsEnabled()` calls do not actually check the overrides
// list and instead return the previously returned value according to the cache
// state in the base::Feature itself. See `base::Feature::cached_value` and
// `base::FeatureList::caching_context_`.
//
// This caching mechanism breaks isolation between our unit tests, so we need
// to flush the cache between tests. We can do that by making sure that the
// feature list caching context (basically, a cache generation counter) is set
// to a value that is different from the one it had in the last
// base::FeatureList::IsEnabled() call. This will cause a cache miss on the next
// base::FeatureList::IsEnabled() call. If we don't do anything this won't be
// the case, as the code under test always installs a base::FeatureList with a
// caching context value of 1 (which is the default).
//
// base::test::ScopedFeatureList uses the exact same approach internally, but we
// can't piggy-back on that because we need to change the caching context of the
// base::FeatureList that is instantiated by the code under test, not the one
// that base::test::ScopedFeatureList installs.
void FlushBaseFeatureCache() {
  auto* const feature_list = base::FeatureList::GetInstance();
  if (feature_list == nullptr) {
    return;
  }
  // Start at 32768 to avoid collisions with ScopedFeatureList's own
  // `g_current_caching_context` (which starts at 1).
  static uint16_t g_feature_list_caching_context = 32768;
  feature_list->SetCachingContextForTesting(g_feature_list_caching_context++);
}

constexpr char kTestFeatureDisabledByDefaultName[] =
    "CronetBaseFeatureTestFeatureDisabledByDefault";
BASE_FEATURE(kTestFeatureDisabledByDefault,
             kTestFeatureDisabledByDefaultName,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kTestFeatureEnabledByDefaultName[] =
    "CronetBaseFeatureTestFeatureEnabledByDefault";
BASE_FEATURE(kTestFeatureEnabledByDefault,
             kTestFeatureEnabledByDefaultName,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kTestFeatureWithParamsName[] =
    "CronetBaseFeatureTestFeatureWithParams";
BASE_FEATURE(kTestFeatureWithParams,
             kTestFeatureWithParamsName,
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr char kTestParamName[] = "test_param";
constexpr char kTestParamDefaultValue[] = "test_param_default_value";
constexpr base::FeatureParam<std::string> kTestParam{
    &kTestFeatureWithParams, kTestParamName, kTestParamDefaultValue};

TEST(ApplyBaseFeatureOverrides, NoOpOnEmptyOverrides) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  ApplyBaseFeatureOverrides(BaseFeatureOverrides());
  FlushBaseFeatureCache();
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
}

TEST(ApplyBaseFeatureOverrides, OverridesFeatureToEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  (*overrides.mutable_feature_states())[kTestFeatureDisabledByDefaultName]
      .set_enabled(true);
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST(ApplyBaseFeatureOverrides, OverridesFeatureToDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  (*overrides.mutable_feature_states())[kTestFeatureEnabledByDefaultName]
      .set_enabled(false);
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
}

TEST(ApplyBaseFeatureOverrides, DoesNotOverrideFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  (*overrides.mutable_feature_states())[kTestFeatureEnabledByDefaultName];
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
}

TEST(ApplyBaseFeatureOverrides, NoOpIfBaseFeatureAlreadyInitialized) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  (*overrides.mutable_feature_states())[kTestFeatureDisabledByDefaultName]
      .set_enabled(true);
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST(ApplyBaseFeatureOverrides,
     DoesNotAssociateFeatureParamsIfNoParamsAreProvided) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  (*overrides.mutable_feature_states())[kTestFeatureWithParamsName].set_enabled(
      true);
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  base::FieldTrialParams params;
  EXPECT_FALSE(
      base::GetFieldTrialParamsByFeature(kTestFeatureWithParams, &params));
  EXPECT_THAT(params, IsEmpty());
  EXPECT_EQ(kTestParam.Get(), kTestParamDefaultValue);
}

TEST(ApplyBaseFeatureOverrides, AssociatesFeatureParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  BaseFeatureOverrides overrides;
  constexpr char kParamValue[] = "test_param_value";
  auto& feature_state =
      (*overrides.mutable_feature_states())[kTestFeatureWithParamsName];
  feature_state.set_enabled(true);
  (*feature_state.mutable_params())[kTestParamName] = kParamValue;
  ApplyBaseFeatureOverrides(overrides);
  FlushBaseFeatureCache();
  base::FieldTrialParams params;
  EXPECT_TRUE(
      base::GetFieldTrialParamsByFeature(kTestFeatureWithParams, &params));
  EXPECT_THAT(params, UnorderedElementsAre(Pair(kTestParamName, kParamValue)));
  EXPECT_EQ(kTestParam.Get(), kParamValue);
}

}  // namespace
}  // namespace cronet
