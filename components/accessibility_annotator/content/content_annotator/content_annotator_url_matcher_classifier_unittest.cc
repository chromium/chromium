// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

TEST(ContentAnnotatorUrlMatcherClassifierTest, Classify_Match) {
  auto classifier = ContentAnnotatorUrlMatcherClassifier::Create(
      R"Json({
        "Education Page":[".*/education(?:[/?#].*|$)"],
        "Checkout Page":[".*/checkout(?:[/?#].*|$)"]
      })Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify(GURL("https://www.example.com/education/123")),
            "Education Page");
  EXPECT_EQ(
      classifier->Classify(GURL("https://www.example.com/checkout?q=test")),
      "Checkout Page");
  EXPECT_EQ(
      classifier->Classify(GURL("https://www.example.com/education#fragment")),
      "Education Page");
  EXPECT_EQ(classifier->Classify(GURL("https://www.example.com/checkout/")),
            "Checkout Page");
}

TEST(ContentAnnotatorUrlMatcherClassifierTest, Classify_NoMatch) {
  auto classifier = ContentAnnotatorUrlMatcherClassifier::Create(
      R"Json({
        "Education Page":[".*/education(?:[/?#].*|$)"],
        "Checkout Page":[".*/checkout(?:[/?#].*|$)"]
      })Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify(GURL("https://www.test.com")), std::nullopt);
  EXPECT_EQ(
      classifier->Classify(GURL("https://www.example.com/add-to-checkout/123")),
      std::nullopt);
  EXPECT_EQ(classifier->Classify(
                GURL("https://www.example.com/rax?cartfdsdfadsfasdf")),
            std::nullopt);
}

TEST(ContentAnnotatorUrlMatcherClassifierTest, Classify_MultipleMatches) {
  auto classifier = ContentAnnotatorUrlMatcherClassifier::Create(
      R"Json({
        "category1":[".*.google.com.*"],
        "category2":["www.google.*"]
      })Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(
      classifier->Classify(GURL("https://www.google.com/cart/123?q=test")),
      "category1");
}

TEST(ContentAnnotatorUrlMatcherClassifierTest, Classify_EmptyUrl) {
  auto classifier = ContentAnnotatorUrlMatcherClassifier::Create(
      R"Json({"Education Page":[".*/education(?:[/?#].*|$)"]})Json");
  ASSERT_NE(classifier, nullptr);
  EXPECT_EQ(classifier->Classify(GURL("")), std::nullopt);
}

TEST(ContentAnnotatorUrlMatcherClassifierTest, Create_InvalidJson) {
  EXPECT_EQ(ContentAnnotatorUrlMatcherClassifier::Create("invalid json"),
            nullptr);
}

}  // namespace accessibility_annotator
