// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/multistep_filter/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

class MultistepFilterUtilTest : public testing::Test {
 public:
  MultistepFilterUtilTest() = default;
};

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kMultistepFilter);

  EXPECT_FALSE(IsUrlAllowed(GURL("https://example.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_FeatureEnabledNoParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kMultistepFilter);

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_Wildcard) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "*"}});

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://google.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_WildcardWithSpaces) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "  *  "}});

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://google.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_WildcardAndOtherDomains) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com, * , test.org"}});

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://google.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_SpecificDomain) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
  // Should match subdomains (eTLD+1 matching).
  EXPECT_TRUE(IsUrlAllowed(GURL("https://www.example.com")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://sub.example.com")));

  // Should not match other domains.
  EXPECT_FALSE(IsUrlAllowed(GURL("https://notexample.com")));
  EXPECT_FALSE(IsUrlAllowed(GURL("https://example.org")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_MultipleDomains) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com, TEST.org"}});

  EXPECT_TRUE(IsUrlAllowed(GURL("https://example.com")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://test.org")));
  EXPECT_TRUE(IsUrlAllowed(GURL("https://sub.test.org")));

  EXPECT_FALSE(IsUrlAllowed(GURL("https://other.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_Empty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", ""}});

  EXPECT_FALSE(IsUrlAllowed(GURL("https://example.com")));
}

TEST_F(MultistepFilterUtilTest, IsUrlAllowed_FallbackToHostForIP) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "localhost, 127.0.0.1"}});

  // eTLD+1 returns empty for IPs and localhost, so we expect it to fall back to
  // host().
  EXPECT_TRUE(IsUrlAllowed(GURL("http://localhost:8080/path")));
  EXPECT_TRUE(IsUrlAllowed(GURL("http://127.0.0.1:8080/path")));

  EXPECT_FALSE(IsUrlAllowed(GURL("http://192.168.1.1")));
}

}  // namespace multistep_filter
