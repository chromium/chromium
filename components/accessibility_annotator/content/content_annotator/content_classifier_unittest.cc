// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include "base/strings/string_number_conversions.h"
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
  struct ClassifierTestOptions {
    std::string title_keyword_rules;
    std::string url_match_rules;
    std::optional<double> sensitivity_threshold;
  };

  std::unique_ptr<ContentClassifier> CreateClassifier(
      const ClassifierTestOptions& options = {}) {
    base::FieldTrialParams params;
    if (!options.title_keyword_rules.empty()) {
      params[kContentAnnotatorClassifierTitleKeywordRules.name] =
          options.title_keyword_rules;
    }
    if (!options.url_match_rules.empty()) {
      params[kContentAnnotatorClassifierUrlMatchRules.name] =
          options.url_match_rules;
    }

    if (options.sensitivity_threshold.has_value()) {
      params[kContentAnnotatorSensitivityThreshold.name] =
          base::NumberToString(*options.sensitivity_threshold);
    }

    feature_list_.InitAndEnableFeatureWithParameters(kContentAnnotator, params);
    return ContentClassifier::Create();
  }

  static ContentClassificationInput CreateDefaultInput() {
    ContentClassificationInput input(GURL("https://www.example.com"));
    input.page_title = "Default Title";
    input.adopted_language = "en";
    input.sensitivity_score = 0.1f;
    return input;
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContentClassifierTest, Classify_AllClassifiersMatch) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules =
           R"JSON({"category_1":["example 1","example 2","example 3"]})JSON",
       .url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON"});
  ASSERT_TRUE(classifier);
  ContentClassificationInput input = CreateDefaultInput();
  input.url = GURL("https://example.com/rule_1");
  input.page_title = "This is example 1";

  ContentClassificationResult result = classifier->Classify(input);

  ASSERT_TRUE(result.title_keyword_result.has_value());
  EXPECT_EQ(result.title_keyword_result->category, "category_1");

  ASSERT_TRUE(result.url_match_result.has_value());
  EXPECT_EQ(result.url_match_result->category, "category_1");
}

TEST_F(ContentClassifierTest, Classify_TitleMatchOnlyEnabled) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules =
           R"JSON({"category_1":["rule 1","rule 2","rule 3"]})JSON"});
  ASSERT_TRUE(classifier);
  ContentClassificationInput input = CreateDefaultInput();
  input.url = GURL("https://example.com/random_page");
  input.page_title = "This is Rule 1";

  ContentClassificationResult result = classifier->Classify(input);

  ASSERT_TRUE(result.title_keyword_result.has_value());
  EXPECT_EQ(result.title_keyword_result->category, "category_1");

  EXPECT_FALSE(result.url_match_result.has_value());
}

TEST_F(ContentClassifierTest, Classify_UrlMatchOnlyEnabled) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_2","/rule_3"]})JSON"});
  ASSERT_TRUE(classifier);
  ContentClassificationInput input = CreateDefaultInput();
  input.url = GURL("https://example.com/rule_2");
  input.page_title = "Complete your transaction";

  ContentClassificationResult result = classifier->Classify(input);

  EXPECT_FALSE(result.title_keyword_result.has_value());

  ASSERT_TRUE(result.url_match_result.has_value());
  EXPECT_EQ(result.url_match_result->category, "category_1");
}

TEST_F(ContentClassifierTest, Classify_NoMatch) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules =
           R"JSON({"category_1":["example 1","example 2","example 3"]})JSON",
       .url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON"});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/blog");
    input.page_title = "My latest thoughts";

    ContentClassificationResult result = classifier->Classify(input);

    ASSERT_TRUE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.title_keyword_result->category.has_value());
    ASSERT_TRUE(result.url_match_result.has_value());
    EXPECT_FALSE(result.url_match_result->category.has_value());
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/blog");

    ContentClassificationResult result = classifier->Classify(input);

    ASSERT_TRUE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.title_keyword_result->category.has_value());
    ASSERT_TRUE(result.url_match_result.has_value());
    EXPECT_FALSE(result.url_match_result->category.has_value());
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("");
    input.page_title = "";

    ContentClassificationResult result = classifier->Classify(input);

    EXPECT_FALSE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.url_match_result.has_value());
  }
}

TEST_F(ContentClassifierTest,
       Create_InvalidTitleKeywordRules_ValidUrlMatchRules) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules = "invalid json",
       .url_match_rules = R"JSON({"category_1":["/rule_1"]})JSON"});
  EXPECT_FALSE(classifier);
}

TEST_F(ContentClassifierTest,
       Create_ValidTitleKeywordRules_InvalidUrlMatchRules) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules = R"JSON({"category_1":["example 1"]})JSON",
       .url_match_rules = "invalid json"});
  EXPECT_FALSE(classifier);
}

TEST_F(ContentClassifierTest, Create_NoRules) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier();
  EXPECT_TRUE(classifier);
}

TEST_F(ContentClassifierTest, Create_BothRulesInvalid) {
  std::unique_ptr<ContentClassifier> classifier =
      CreateClassifier({.title_keyword_rules = "invalid json",
                        .url_match_rules = "invalid json"});
  EXPECT_FALSE(classifier);
}

TEST_F(ContentClassifierTest, Classify_LanguageCheck) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON"});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "en";
    ContentClassificationResult result = classifier->Classify(input);
    // Classifiers are run as the language check is passed.
    EXPECT_TRUE(result.url_match_result.has_value());
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "en-US";
    ContentClassificationResult result = classifier->Classify(input);
    // Classifiers are run as the language check is passed.
    EXPECT_TRUE(result.url_match_result.has_value());
  }
}

TEST_F(ContentClassifierTest,
       Classify_LanguageCheckFailsNoFurtherClassification) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON"});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "de";
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_FALSE(result.url_match_result.has_value());
  }
}

TEST_F(ContentClassifierTest, Classify_SensitivityCheck) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON",
       .sensitivity_threshold = 0.5});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.sensitivity_score = 0.4f;
    ContentClassificationResult result = classifier->Classify(input);
    // Classifiers are run as the sensitivity check is passed.
    EXPECT_TRUE(result.url_match_result.has_value());
  }
}

TEST_F(ContentClassifierTest,
       Classify_SensitivityCheckFailsNoFurtherClassification) {
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON",
       .sensitivity_threshold = 0.5});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.sensitivity_score = 0.6f;
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_FALSE(result.url_match_result.has_value());
  }
}

}  // namespace

}  // namespace accessibility_annotator
