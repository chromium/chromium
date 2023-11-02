// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ntp_snippets {

namespace {
const char kExpectedZineURL[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kTestZineURL[] = "https://test.google.com/";
}  // namespace

TEST(FeaturesTest, GetContentSuggestionsReferrerURL_DefaultValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kArticleSuggestionsFeature);
  EXPECT_EQ(kExpectedZineURL, GetContentSuggestionsReferrerURL());

  // In code this will be often used inside of a GURL.
  EXPECT_EQ(kExpectedZineURL, GURL(GetContentSuggestionsReferrerURL()));
  EXPECT_EQ(kExpectedZineURL, GURL(GetContentSuggestionsReferrerURL()).spec());
}

TEST(FeaturesTest, GetContentSuggestionsReferrerURL_ParamValue) {
  base::test::ScopedFeatureList feature_list;

  std::map<std::string, std::string> parameters;
  parameters["referrer_url"] = kTestZineURL;
  feature_list.InitAndEnableFeatureWithParameters(kArticleSuggestionsFeature,
                                                  parameters);
  EXPECT_EQ(kTestZineURL, GetContentSuggestionsReferrerURL());
}

}  // namespace ntp_snippets
