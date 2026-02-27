// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/hashing.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

namespace {

using ClassifierResultStatus = ContentClassifier::ClassifierResultStatus;

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
    std::string relevance_values;
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
    if (!options.relevance_values.empty()) {
      params[kContentAnnotatorClassifierRelevanceValues.name] =
          options.relevance_values;
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

  struct ExpectHistogramsOptions {
    bool language_passed = true;
    bool sensitivity_passed = true;
    std::string_view title_category = "";
    std::string_view url_category = "";
    int expected_input_count = 1;
    // The expected count for classifier results. If not set,
    // `expected_input_count` is used.
    std::optional<int> classifier_result_expected_count = std::nullopt;
  };

  static void ExpectHistograms(base::HistogramTester& histogram_tester,
                               const ExpectHistogramsOptions& options) {
    histogram_tester.ExpectUniqueSample("AccessibilityAnnotator.LanguageCheck",
                                        options.language_passed,
                                        options.expected_input_count);
    histogram_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.SensitivityCheck", options.sensitivity_passed,
        options.expected_input_count);

    int classifier_count = options.classifier_result_expected_count.value_or(
        options.expected_input_count);
    if (!options.title_category.empty()) {
      histogram_tester.ExpectUniqueSample(
          "AccessibilityAnnotator.TitleKeywordClassifierResult",
          variations::HashName(options.title_category), classifier_count);
    }
    if (!options.url_category.empty()) {
      histogram_tester.ExpectUniqueSample(
          "AccessibilityAnnotator.UrlClassifierResult",
          variations::HashName(options.url_category), classifier_count);
    }
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ContentClassifierTest, Classify_AllClassifiersMatch) {
  base::HistogramTester histogram_tester;
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

  ExpectHistograms(histogram_tester, {.title_category = "category_1",
                                      .url_category = "category_1"});
}

TEST_F(ContentClassifierTest, Classify_TitleMatchOnlyEnabled) {
  base::HistogramTester histogram_tester;
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
  ExpectHistograms(
      histogram_tester,
      {.title_category = "category_1",
       .url_category = ContentClassifier::ClassifierResultStatusToString(
           ClassifierResultStatus::kDidNotRunMissingClassifier)});
}

TEST_F(ContentClassifierTest, Classify_UrlMatchOnlyEnabled) {
  base::HistogramTester histogram_tester;
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
  ExpectHistograms(
      histogram_tester,
      {.title_category = ContentClassifier::ClassifierResultStatusToString(
           ClassifierResultStatus::kDidNotRunMissingClassifier),
       .url_category = "category_1"});
}

TEST_F(ContentClassifierTest, Classify_NoMatch) {
  base::HistogramTester histogram_tester;
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

    ExpectHistograms(
        histogram_tester,
        {.title_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kInconclusiveNoMatch),
         .url_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kInconclusiveNoMatch)});
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/blog");

    ContentClassificationResult result = classifier->Classify(input);

    ASSERT_TRUE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.title_keyword_result->category.has_value());
    ASSERT_TRUE(result.url_match_result.has_value());
    EXPECT_FALSE(result.url_match_result->category.has_value());

    ExpectHistograms(
        histogram_tester,
        {.title_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kInconclusiveNoMatch),
         .url_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kInconclusiveNoMatch),
         .expected_input_count = 2});
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("");
    input.page_title = "";

    ContentClassificationResult result = classifier->Classify(input);

    EXPECT_FALSE(result.title_keyword_result.has_value());
    EXPECT_FALSE(result.url_match_result.has_value());

    histogram_tester.ExpectBucketCount(
        "AccessibilityAnnotator.TitleKeywordClassifierResult",
        variations::HashName(ContentClassifier::ClassifierResultStatusToString(
            ClassifierResultStatus::kDidNotRunEmptyPageTitle)),
        1);
    histogram_tester.ExpectTotalCount(
        "AccessibilityAnnotator.TitleKeywordClassifierResult", 3);
    histogram_tester.ExpectBucketCount(
        "AccessibilityAnnotator.UrlClassifierResult",
        variations::HashName(ContentClassifier::ClassifierResultStatusToString(
            ClassifierResultStatus::kDidNotRunInvalidUrl)),
        1);
    histogram_tester.ExpectTotalCount(
        "AccessibilityAnnotator.UrlClassifierResult", 3);
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
  base::HistogramTester histogram_tester;
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON"});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "en";
    input.page_title = "";
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_TRUE(result.url_match_result.has_value());
    ExpectHistograms(
        histogram_tester,
        {.title_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kDidNotRunMissingClassifierEmptyPageTitle),
         .url_category = "category_1"});
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "en-US";
    input.page_title = "";
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_TRUE(result.url_match_result.has_value());
    ExpectHistograms(
        histogram_tester,
        {.title_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kDidNotRunMissingClassifierEmptyPageTitle),
         .url_category = "category_1",
         .expected_input_count = 2});
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.adopted_language = "de";
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_TRUE(result.url_match_result.has_value());
    histogram_tester.ExpectBucketCount("AccessibilityAnnotator.LanguageCheck",
                                       false, 1);
    histogram_tester.ExpectTotalCount("AccessibilityAnnotator.LanguageCheck",
                                      3);
    histogram_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.UrlClassifierResult",
        variations::HashName("category_1"), 3);
  }
}

TEST_F(ContentClassifierTest, Classify_SensitivityCheck) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.url_match_rules = R"JSON({"category_1":["/rule_1","/rule_2"]})JSON",
       .sensitivity_threshold = 0.5});
  ASSERT_TRUE(classifier);

  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.sensitivity_score = 0.4f;
    input.page_title = "";
    ContentClassificationResult result = classifier->Classify(input);
    // Classifiers are run as the sensitivity check is passed.
    EXPECT_TRUE(result.url_match_result.has_value());
    ExpectHistograms(
        histogram_tester,
        {.title_category = ContentClassifier::ClassifierResultStatusToString(
             ClassifierResultStatus::kDidNotRunMissingClassifierEmptyPageTitle),
         .url_category = "category_1"});
  }
  {
    ContentClassificationInput input = CreateDefaultInput();
    input.url = GURL("https://example.com/rule_1");
    input.sensitivity_score = 0.6f;
    ContentClassificationResult result = classifier->Classify(input);
    EXPECT_TRUE(result.url_match_result.has_value());
    histogram_tester.ExpectBucketCount(
        "AccessibilityAnnotator.SensitivityCheck", false, 1);
    histogram_tester.ExpectTotalCount("AccessibilityAnnotator.SensitivityCheck",
                                      2);
    histogram_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.UrlClassifierResult",
        variations::HashName("category_1"), 2);
  }
}

TEST_F(ContentClassifierTest, Classify_LogsUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::unique_ptr<ContentClassifier> classifier = CreateClassifier(
      {.title_keyword_rules = R"JSON({"category_1":["rule1"]})JSON",
       .url_match_rules = R"JSON({"category_2":["/rule1"]})JSON",
       .relevance_values = R"JSON({"category_1":3,"category_2":2})JSON"});
  ASSERT_TRUE(classifier);

  ContentClassificationInput input(GURL("https://example.com/rule1"));
  input.ukm_source_id = ukm::AssignNewSourceId();
  input.page_title = "rule1";
  input.adopted_language = "en";
  input.sensitivity_score = 0.1f;

  classifier->Classify(input);

  using UkmEntry =
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto entry = entries[0];
  ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kIsTargetLanguageName, true);
  ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kTitleKeywordResultName, 3);
  ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kUrlMatchResultName, 2);
}

TEST_F(ContentClassifierTest, Classify_LogsUkm_NoMatch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::unique_ptr<ContentClassifier> classifier =
      CreateClassifier({.title_keyword_rules = R"JSON({"cat1":["rule1"]})JSON",
                        .relevance_values = R"JSON({"cat1":3})JSON"});
  ASSERT_TRUE(classifier);

  ContentClassificationInput input(GURL("https://example.com/other"));
  input.ukm_source_id = ukm::AssignNewSourceId();
  input.page_title = "other";
  input.adopted_language = "fr";
  input.sensitivity_score = 0.1f;

  classifier->Classify(input);

  using UkmEntry =
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto entry = entries[0];
  ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kIsTargetLanguageName, false);
  ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kTitleKeywordResultName, 1);
  EXPECT_FALSE(
      ukm_recorder.GetEntryMetric(entry, UkmEntry::kUrlMatchResultName));
}

}  // namespace

}  // namespace accessibility_annotator
