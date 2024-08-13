// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {

namespace {

TEST(UserAnnotationsFeaturesTest, ShouldAddFormSubmissionForURL) {
  // Feature enabled with host not in allowlist
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kUserAnnotations,
        {{"allowed_hosts_for_form_submissions", "example.com,otherhost.com"}});
    EXPECT_FALSE(
        ShouldAddFormSubmissionForURL(GURL("https://notinlist.com/whatever")));
  }

  // Feature enabled with host explicitly in allowlist
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kUserAnnotations,
        {{"allowed_hosts_for_form_submissions", "example.com,otherhost.com"}});
    EXPECT_TRUE(
        ShouldAddFormSubmissionForURL(GURL("https://example.com/whatever")));
  }

  // Feature enabled with wildcard param.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kUserAnnotations, {{"allowed_hosts_for_form_submissions", "*"}});
    EXPECT_TRUE(ShouldAddFormSubmissionForURL(GURL("https://example.com")));
  }

  // Feature enablement not specified - param not specified.
  { EXPECT_TRUE(ShouldAddFormSubmissionForURL(GURL("https://example.com"))); }
}

}  // namespace
}  // namespace user_annotations
