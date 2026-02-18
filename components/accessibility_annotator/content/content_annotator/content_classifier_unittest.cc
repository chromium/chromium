// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

TEST(ContentClassificationInputTest, IsComplete) {
  ContentClassificationInput complete_input(GURL("https://www.example.com"));
  complete_input.sensitivity_score = 0.1f;
  complete_input.navigation_timestamp = base::Time::Now();
  complete_input.adopted_language = "en";
  complete_input.page_title = "Example Page";
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

  ContentClassificationInput empty_input(GURL(""));
  EXPECT_FALSE(empty_input.IsComplete());
}

}  // namespace

}  // namespace accessibility_annotator
