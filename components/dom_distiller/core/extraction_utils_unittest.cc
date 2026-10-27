// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

TEST(ExtractionUtilsTest, TestHeuristicMinScoreAndContentLength) {
  std::string script = GetDistillerTriggeringScript();
  EXPECT_EQ(std::string::npos, script.find("$$MIN_SCORE_PLACEHOLDER"));
  EXPECT_EQ(std::string::npos, script.find("$$MIN_CONTENT_LENGTH_PLACEHOLDER"));
  std::string expected_args =
      "(" + base::NumberToString(kHeuristicMinScore) + ", " +
      base::NumberToString(kHeuristicMinContentLength) + ");";
  EXPECT_NE(std::string::npos, script.find(expected_args));
}

TEST(ExtractionUtilsTest, ParseDistillationResult) {
  base::DictValue dict;
  dict.Set(kDistilledTitleKey, "Article Title");
  dict.Set(kDistilledContentKey, "<p>Article HTML</p>");
  dict.Set(kDistilledTextDirectionKey, "rtl");
  dict.Set(kDistilledTextContentKey, "One two three four");

  DistilledPageProto result;
  EXPECT_TRUE(ParseDistillationResult(base::Value(std::move(dict)), &result));
  EXPECT_EQ("Article Title", result.title());
  EXPECT_EQ("<p>Article HTML</p>", result.html());
  EXPECT_EQ("rtl", result.text_direction());
  EXPECT_EQ("One two three four", result.text_content());
  EXPECT_EQ(4, result.word_count());
}

TEST(ExtractionUtilsTest, ParseDistillationResultDefaults) {
  {
    base::DictValue dict;
    DistilledPageProto result;
    EXPECT_TRUE(ParseDistillationResult(base::Value(std::move(dict)), &result));
    EXPECT_EQ("auto", result.text_direction());
    EXPECT_FALSE(result.has_word_count());
  }

  {
    base::Value non_dict("not a dict");
    DistilledPageProto result;
    EXPECT_FALSE(ParseDistillationResult(non_dict, &result));
  }
}

}  // namespace dom_distiller
