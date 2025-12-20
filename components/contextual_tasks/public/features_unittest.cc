// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(FeaturesTest, ForcedEmbeddedPageHost_NoOverride) {
  ASSERT_EQ("", GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToGoogleHost) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"forced-embedded-page-host", "corp.google.com"}});

  ASSERT_EQ("corp.google.com", GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"forced-embedded-page-host", "example.com"}});

  ASSERT_EQ("", GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost_BadSuffix) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"forced-embedded-page-host", "corpgoogle.com"}});

  ASSERT_EQ("", GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost_Subdomain) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"forced-embedded-page-host", "google.com.example.com"}});

  ASSERT_EQ("", GetForcedEmbeddedPageHost());
}

}  // namespace contextual_tasks
