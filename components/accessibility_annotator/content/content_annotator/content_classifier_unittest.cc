// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

namespace {

TEST(ContentClassificationInputTest, IsComplete) {
  ContentClassificationInput complete_input(GURL("https://www.example.com"));
  complete_input.sensitivity_score = 0.1f;
  complete_input.navigation_timestamp = base::Time::Now();
  complete_input.adopted_language = "en";
  complete_input.page_title = "Example Page";
  scoped_refptr<
      base::RefCountedData<optimization_guide::proto::AnnotatedPageContent>>
      annotated_page_content = base::MakeRefCounted<base::RefCountedData<
          optimization_guide::proto::AnnotatedPageContent>>();
  annotated_page_content->data.mutable_main_frame_data()->set_title(
      "Test Title");
  complete_input.annotated_page_content = std::move(annotated_page_content);
  EXPECT_TRUE(complete_input.IsComplete());

  {
    ContentClassificationInput input = complete_input;
    input.sensitivity_score.reset();
    EXPECT_FALSE(input.IsComplete());
  }
  {
    ContentClassificationInput input = complete_input;
    input.navigation_timestamp.reset();
    EXPECT_FALSE(input.IsComplete());
  }
  {
    ContentClassificationInput input = complete_input;
    input.adopted_language.reset();
    EXPECT_FALSE(input.IsComplete());
  }
  {
    ContentClassificationInput input = complete_input;
    input.page_title.reset();
    EXPECT_FALSE(input.IsComplete());
  }
  {
    ContentClassificationInput input = complete_input;
    input.annotated_page_content.reset();
    EXPECT_FALSE(input.IsComplete());
  }

  ContentClassificationInput empty_input(GURL(""));
  EXPECT_FALSE(empty_input.IsComplete());
}

class ContentClassifierTest : public testing::Test {
 protected:
  std::unique_ptr<ContentClassifier> CreateClassifier(
      const std::string& title_keyword_rules,
      const std::string& url_match_rules) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kContentAnnotator,
        {{kContentAnnotatorClassifierTitleKeywordRules.name,
          title_keyword_rules},
         {kContentAnnotatorClassifierUrlMatchRules.name, url_match_rules}});
    return ContentClassifier::Create();
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContentClassifierTest, Classify_AllClassifiersMatch) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      R"JSON({"category_1":["example 1","example 2","example 3"]})JSON",
      R"JSON({"category_1":["/rule_1","/rule_2"]})JSON");
  ASSERT_TRUE(classifier);
  ContentClassificationInput input(GURL("https://example.com/rule_1"));
  input.page_title = "This is example 1";

  ContentClassificationResult result = classifier->Classify(input);

  ASSERT_TRUE(result.title_keyword_result.has_value());
  EXPECT_EQ(result.title_keyword_result->category, "category_1");

  ASSERT_TRUE(result.url_match_result.has_value());
  EXPECT_EQ(result.url_match_result->category, "category_1");
}

TEST_F(ContentClassifierTest, Classify_TitleMatchOnlyEnabled) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      R"JSON({"category_1":["rule 1","rule 2","rule 3"]})JSON", "");
  ASSERT_TRUE(classifier);
  ContentClassificationInput input(GURL("https://example.com/random_page"));
  input.page_title = "This is Rule 1";

  ContentClassificationResult result = classifier->Classify(input);

  ASSERT_TRUE(result.title_keyword_result.has_value());
  EXPECT_EQ(result.title_keyword_result->category, "category_1");

  EXPECT_FALSE(result.url_match_result.has_value());
}

TEST_F(ContentClassifierTest, Classify_UrlMatchOnlyEnabled) {
  std::unique_ptr<ContentClassifier> classifier =
      CreateClassifier("", R"JSON({"category_1":["/rule_2","/rule_3"]})JSON");
  ASSERT_TRUE(classifier);
  ContentClassificationInput input(GURL("https://example.com/rule_2"));
  input.page_title = "Complete your transaction";

  ContentClassificationResult result = classifier->Classify(input);

  EXPECT_FALSE(result.title_keyword_result.has_value());

  ASSERT_TRUE(result.url_match_result.has_value());
  EXPECT_EQ(result.url_match_result->category, "category_1");
}

TEST_F(ContentClassifierTest, Classify_NoMatch) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      R"JSON({"category_1":["example 1","example 2","example 3"]})JSON",
      R"JSON({"category_1":["/rule_1","/rule_2"]})JSON");
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input(GURL("https://example.com/blog"));
    input.page_title = "My latest thoughts";

    ContentClassificationResult result = classifier->Classify(input);

    ASSERT_TRUE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.title_keyword_result->category.has_value());
    ASSERT_TRUE(result.url_match_result.has_value());
    EXPECT_FALSE(result.url_match_result->category.has_value());
  }
  {
    ContentClassificationInput input(GURL("https://example.com/blog"));
    ContentClassificationResult result = classifier->Classify(input);

    EXPECT_FALSE(result.title_keyword_result.has_value());
    ASSERT_TRUE(result.url_match_result.has_value());
    EXPECT_FALSE(result.url_match_result->category.has_value());
  }
  {
    ContentClassificationInput input(GURL(""));
    input.page_title = "";
    ContentClassificationResult result = classifier->Classify(input);

    EXPECT_FALSE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.url_match_result.has_value());
  }
}

TEST_F(ContentClassifierTest,
       Create_InvalidTitleKeywordRules_ValidUrlMatchRules) {
  std::unique_ptr<ContentClassifier> classifier =
      CreateClassifier("invalid json", R"JSON({"category_1":["/rule_1"]})JSON");
  EXPECT_FALSE(classifier);
}

TEST_F(ContentClassifierTest,
       Create_ValidTitleKeywordRules_InvalidUrlMatchRules) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      R"JSON({"category_1":["example 1"]})JSON", "invalid json");
  EXPECT_FALSE(classifier);
}

TEST_F(ContentClassifierTest, Create_NoRules) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier("", "");
  EXPECT_TRUE(classifier);
}

TEST_F(ContentClassifierTest, Create_BothRulesInvalid) {
  std::unique_ptr<ContentClassifier> classifier =
      CreateClassifier("invalid json", "invalid json");
  EXPECT_FALSE(classifier);
}

}  // namespace

}  // namespace accessibility_annotator
