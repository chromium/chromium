// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_text_utils.h"

#include "pdf/accessibility_structs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf {

namespace {

using chrome_pdf::AccessibilityTextDirection;
using chrome_pdf::AccessibilityTextRunInfo;

TEST(PdfAccessibilityTextUtilsTest, IsRightToLeftRun) {
  AccessibilityTextRunInfo run;
  run.direction = AccessibilityTextDirection::kRightToLeft;
  EXPECT_TRUE(IsRightToLeftRun(run));
  run.direction = AccessibilityTextDirection::kNone;
  EXPECT_FALSE(IsRightToLeftRun(run));
  run.direction = AccessibilityTextDirection::kLeftToRight;
  EXPECT_FALSE(IsRightToLeftRun(run));
  run.direction = AccessibilityTextDirection::kTopToBottom;
  EXPECT_FALSE(IsRightToLeftRun(run));
  run.direction = AccessibilityTextDirection::kBottomToTop;
  EXPECT_FALSE(IsRightToLeftRun(run));
}

TEST(PdfAccessibilityTextUtilsTest, IsVerticalTextDirection) {
  EXPECT_TRUE(
      IsVerticalTextDirection(AccessibilityTextDirection::kTopToBottom));
  EXPECT_TRUE(
      IsVerticalTextDirection(AccessibilityTextDirection::kBottomToTop));
  EXPECT_FALSE(IsVerticalTextDirection(AccessibilityTextDirection::kNone));
  EXPECT_FALSE(
      IsVerticalTextDirection(AccessibilityTextDirection::kLeftToRight));
  EXPECT_FALSE(
      IsVerticalTextDirection(AccessibilityTextDirection::kRightToLeft));
}

TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralValid) {
  EXPECT_EQ(1, ParseRomanNumeral("i"));
  EXPECT_EQ(4, ParseRomanNumeral("iv"));
  EXPECT_EQ(9, ParseRomanNumeral("ix"));
  EXPECT_EQ(14, ParseRomanNumeral("xiv"));
  EXPECT_EQ(38, ParseRomanNumeral("xxxviii"));
  EXPECT_EQ(39, ParseRomanNumeral("xxxix"));
  EXPECT_EQ(1, ParseRomanNumeral("I"));
  EXPECT_EQ(4, ParseRomanNumeral("IV"));
  EXPECT_EQ(12, ParseRomanNumeral("XII"));
}

TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralNonStandard) {
  EXPECT_FALSE(ParseRomanNumeral("iiii").has_value());
  EXPECT_FALSE(ParseRomanNumeral("vv").has_value());
  EXPECT_FALSE(ParseRomanNumeral("vx").has_value());
  EXPECT_FALSE(ParseRomanNumeral("iix").has_value());
  EXPECT_FALSE(ParseRomanNumeral("viv").has_value());
  EXPECT_FALSE(ParseRomanNumeral("ixi").has_value());
  EXPECT_FALSE(ParseRomanNumeral("IIII").has_value());
}

TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralMixedCase) {
  EXPECT_FALSE(ParseRomanNumeral("iV").has_value());
  EXPECT_FALSE(ParseRomanNumeral("Iv").has_value());
  EXPECT_FALSE(ParseRomanNumeral("Xii").has_value());
}

// Values above `kMaxRomanNumeralValue` aren't supported, e.g. "xl", "mix" or
// "CD".
TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralAboveMax) {
  EXPECT_FALSE(ParseRomanNumeral("xxxx").has_value());
  EXPECT_FALSE(ParseRomanNumeral("xl").has_value());
  EXPECT_FALSE(ParseRomanNumeral("l").has_value());
  EXPECT_FALSE(ParseRomanNumeral("mix").has_value());
  EXPECT_FALSE(ParseRomanNumeral("CD").has_value());
}

// Only the whole text is parsed, so a sentence or word that starts with "I"
// doesn't match.
TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralStartsWithI) {
  EXPECT_FALSE(ParseRomanNumeral("I think so.").has_value());
  EXPECT_FALSE(ParseRomanNumeral("I'm").has_value());
  EXPECT_FALSE(ParseRomanNumeral("If").has_value());
}

TEST(PdfAccessibilityTextUtilsTest, ParseRomanNumeralInvalid) {
  EXPECT_FALSE(ParseRomanNumeral("").has_value());
  EXPECT_FALSE(ParseRomanNumeral("a").has_value());
  EXPECT_FALSE(ParseRomanNumeral("iv.").has_value());
  EXPECT_FALSE(ParseRomanNumeral(" iv").has_value());
  EXPECT_FALSE(ParseRomanNumeral("i v").has_value());
  EXPECT_FALSE(ParseRomanNumeral("1").has_value());
}

}  // namespace

}  // namespace pdf
