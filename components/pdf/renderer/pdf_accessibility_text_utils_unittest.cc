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

}  // namespace

}  // namespace pdf
