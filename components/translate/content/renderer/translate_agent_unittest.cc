// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_agent.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

TEST(TranslateAgentTest, TestBuildTranslationScript) {
  // Test expected cases.
  EXPECT_EQ(TranslateAgent::BuildTranslationScript("en", "es"),
            "cr.googleTranslate.translate(\"en\",\"es\")");
  EXPECT_EQ(TranslateAgent::BuildTranslationScript("en-US", "zh-TW"),
            "cr.googleTranslate.translate(\"en-US\",\"zh-TW\")");

  // Test that quote gets quoted.
  EXPECT_EQ(TranslateAgent::BuildTranslationScript("en\"", "es"),
            "cr.googleTranslate.translate(\"en\\\"\",\"es\")");

  // Test that < gets quoted.
  EXPECT_EQ(TranslateAgent::BuildTranslationScript("en<", "es"),
            "cr.googleTranslate.translate(\"en\\u003C\",\"es\")");
}

}  // namespace translate
