// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_test_harness.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

using mojom::ActivationLevel;

using FlattenedConfig =
    std::tuple<ActivationScope, ActivationList, ActivationLevel>;

class SubresourceFilterConfigurationTest
    : public SubresourceFilterTestHarness,
      public ::testing::WithParamInterface<FlattenedConfig> {};

// Do not configure the URL with Safe Browsing to be part of any list. The only
// time we should filter subresources is if we have ALL_SITES scope.
TEST_P(SubresourceFilterConfigurationTest,
       NoList_UsuallyNoActivation) {
  auto [scope, activation_list, level] = GetParam();
  SCOPED_TRACE(::testing::Message("ActivationScope: ") << scope);
  SCOPED_TRACE(::testing::Message("ActivationList: ") << activation_list);
  SCOPED_TRACE(::testing::Message("ActivationLevel: ") << level);

  const GURL url("https://example.test/");
  scoped_configuration().ResetConfiguration(
      Configuration(level, scope, activation_list));
  SimulateNavigateAndCommit(url, main_rfh());
  if (!CreateAndNavigateDisallowedSubframe(main_rfh())) {
    EXPECT_EQ(scope, ActivationScope::ALL_SITES);
  }
}

TEST_P(SubresourceFilterConfigurationTest, OneListActivation) {
  auto [scope, activation_list, level] = GetParam();
  SCOPED_TRACE(::testing::Message("ActivationScope: ") << scope);
  SCOPED_TRACE(::testing::Message("ActivationList: ") << activation_list);
  SCOPED_TRACE(::testing::Message("ActivationLevel: ") << level);

  const GURL url("https://example.test/");
  ConfigureAsSubresourceFilterOnlyURL(url);
  scoped_configuration().ResetConfiguration(
      Configuration(level, scope, activation_list));
  SimulateNavigateAndCommit(url, main_rfh());
  if (!CreateAndNavigateDisallowedSubframe(main_rfh())) {
    EXPECT_TRUE(scope == ActivationScope::ALL_SITES ||
                (scope == ActivationScope::ACTIVATION_LIST &&
                 activation_list == ActivationList::SUBRESOURCE_FILTER));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubresourceFilterConfigurationTest,
    ::testing::Combine(
        ::testing::Values(ActivationScope::NO_SITES,
                          ActivationScope::ALL_SITES,
                          ActivationScope::ACTIVATION_LIST),
        ::testing::Values(ActivationList::NONE,
                          ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
                          ActivationList::PHISHING_INTERSTITIAL,
                          ActivationList::SUBRESOURCE_FILTER),
        ::testing::Values(ActivationLevel::kEnabled,
                          ActivationLevel::kDisabled,
                          ActivationLevel::kDryRun)));

}  // namespace subresource_filter
