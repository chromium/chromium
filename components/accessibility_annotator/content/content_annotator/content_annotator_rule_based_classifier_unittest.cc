// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

TEST(ContentAnnotatorRuleBasedClassifierTest, Classify_Match) {
  auto classifier = ContentAnnotatorRuleBasedClassifier::Create(
      R"Json({"category1":["\\brule1\\b","\\brule2\\b"]})Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify("this is a Rule1?"), "category1");
  EXPECT_EQ(classifier->Classify("THIS IS A RULE2."), "category1");
}

TEST(ContentAnnotatorRuleBasedClassifierTest, Classify_NoMatch) {
  auto classifier = ContentAnnotatorRuleBasedClassifier::Create(
      R"Json({"category1":["\\brule1\\b","\\brule2\\b"]})Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify("this is a test"), std::nullopt);
  EXPECT_EQ(classifier->Classify("rule1rule2"), std::nullopt);
  EXPECT_EQ(classifier->Classify("rule 2."), std::nullopt);
}

TEST(ContentAnnotatorRuleBasedClassifierTest, Classify_MultipleMatches) {
  auto classifier = ContentAnnotatorRuleBasedClassifier::Create(
      R"Json({"category1":["\\brule1\\b"],"category2":["\\brule2\\b"]})Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify("this is a rule1 and rule2"), "category1");
}

TEST(ContentAnnotatorRuleBasedClassifierTest, Classify_EmptyText) {
  auto classifier = ContentAnnotatorRuleBasedClassifier::Create(
      R"Json({"category1":["\\brule1\\b","\\brule2\\b"]})Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify(""), std::nullopt);
}

TEST(ContentAnnotatorRuleBasedClassifierTest, Create_InvalidRegex) {
  std::string rules_json = R"Json({"category1":["("]})Json";
  EXPECT_EQ(ContentAnnotatorRuleBasedClassifier::Create(rules_json), nullptr);
}

}  // namespace accessibility_annotator
