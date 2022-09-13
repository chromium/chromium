// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_url_feature_extractor.h"

#include <string>
#include <vector>
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;

namespace safe_browsing {

class PhishingUrlFeatureExtractorTest : public ::testing::Test {
 protected:
  PhishingUrlFeatureExtractor extractor_;

  void SplitStringIntoLongAlphanumTokens(const std::string& full,
                                         std::vector<std::string>* tokens) {
    PhishingUrlFeatureExtractor::SplitStringIntoLongAlphanumTokens(full,
                                                                   tokens);
  }

  void FillFeatureMap(size_t count, FeatureMap* features) {
    for (size_t i = 0; i < count; ++i) {
      EXPECT_TRUE(
          features->AddBooleanFeature(base::StringPrintf("Feature%" PRIuS, i)));
    }
  }
};

TEST_F(PhishingUrlFeatureExtractorTest, ExtractFeatures) {
  std::string url = "http://123.0.0.1/mydocuments/a.file.html";
  FeatureMap features;

  // If feature map is already full, features cannot be extracted.
  FillFeatureMap(FeatureMap::kMaxFeatureMapSize, &features);
  ASSERT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));
  features.Clear();

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kUrlHostIsIpAddress);
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("mydocuments"));
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("file"));
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("html"));

  ASSERT_TRUE(extractor_.ExtractFeatures(GURL(url), &features));
  ExpectFeatureMapsAreEqual(features, expected_features);
  // If feature map is already full, features cannot be extracted.
  features.Clear();
  FillFeatureMap(FeatureMap::kMaxFeatureMapSize - 1, &features);
  ASSERT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));
  features.Clear();

  url = "http://www.www.cnn.co.uk/sports/sports/index.html?shouldnotappear";
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kUrlTldToken +
                                      std::string("co.uk"));
  expected_features.AddBooleanFeature(features::kUrlDomainToken +
                                      std::string("cnn"));
  expected_features.AddBooleanFeature(features::kUrlOtherHostToken +
                                      std::string("www"));
  expected_features.AddBooleanFeature(features::kUrlNumOtherHostTokensGTOne);
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("sports"));
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("index"));
  expected_features.AddBooleanFeature(features::kUrlPathToken +
                                      std::string("html"));

  features.Clear();
  ASSERT_TRUE(extractor_.ExtractFeatures(GURL(url), &features));
  ExpectFeatureMapsAreEqual(features, expected_features);
  features.Clear();
  // If feature map is already full, features cannot be extracted.
  FillFeatureMap(FeatureMap::kMaxFeatureMapSize - 5, &features);
  ASSERT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "http://justadomain.com/";
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kUrlTldToken +
                                      std::string("com"));
  expected_features.AddBooleanFeature(features::kUrlDomainToken +
                                      std::string("justadomain"));

  features.Clear();
  ASSERT_TRUE(extractor_.ExtractFeatures(GURL(url), &features));
  ExpectFeatureMapsAreEqual(features, expected_features);
  // If feature map is already full, features cannot be extracted.
  features.Clear();
  FillFeatureMap(FeatureMap::kMaxFeatureMapSize - 1, &features);
  ASSERT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "http://witharef.com/#abc";
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kUrlTldToken +
                                      std::string("com"));
  expected_features.AddBooleanFeature(features::kUrlDomainToken +
                                      std::string("witharef"));

  features.Clear();
  ASSERT_TRUE(extractor_.ExtractFeatures(GURL(url), &features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  url = "http://...www..lotsodots....com./";
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kUrlTldToken +
                                      std::string("com"));
  expected_features.AddBooleanFeature(features::kUrlDomainToken +
                                      std::string("lotsodots"));
  expected_features.AddBooleanFeature(features::kUrlOtherHostToken +
                                      std::string("www"));

  features.Clear();
  ASSERT_TRUE(extractor_.ExtractFeatures(GURL(url), &features));
  ExpectFeatureMapsAreEqual(features, expected_features);
  // If feature map is already full, features cannot be extracted.
  features.Clear();
  FillFeatureMap(FeatureMap::kMaxFeatureMapSize - 2, &features);
  ASSERT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "http://unrecognized.tld/";
  EXPECT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "http://com/123";
  EXPECT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "http://.co.uk/";
  EXPECT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "file:///nohost.txt";
  EXPECT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));

  url = "not:valid:at:all";
  EXPECT_FALSE(extractor_.ExtractFeatures(GURL(url), &features));
}

TEST_F(PhishingUrlFeatureExtractorTest, SplitStringIntoLongAlphanumTokens) {
  std::string full = "This.is/a_pretty\\unusual-!path,indeed";
  std::vector<std::string> long_tokens;
  SplitStringIntoLongAlphanumTokens(full, &long_tokens);
  EXPECT_THAT(long_tokens,
              ElementsAre("This", "pretty", "unusual", "path", "indeed"));

  long_tokens.clear();
  full = "...i-am_re/al&ly\\b,r,o|k=e:n///up%20";
  SplitStringIntoLongAlphanumTokens(full, &long_tokens);
  EXPECT_THAT(long_tokens, ElementsAre());
}

}  // namespace safe_browsing
