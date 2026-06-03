// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

using WindowMetadataControllerTest = testing::Test;

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_Empty) {
  EXPECT_EQ(u"", WindowMetadataController::FormatTitleForDisplay(u""));
}

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_NoNewlines) {
  EXPECT_EQ(u"Hello World",
            WindowMetadataController::FormatTitleForDisplay(u"Hello World"));
}

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_SingleNewline) {
  EXPECT_EQ(u"HelloWorld",
            WindowMetadataController::FormatTitleForDisplay(u"Hello\nWorld"));
}

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_MultipleNewlines) {
  EXPECT_EQ(u"abc",
            WindowMetadataController::FormatTitleForDisplay(u"a\nb\nc"));
}

TEST_F(WindowMetadataControllerTest,
       FormatTitleForDisplay_ConsecutiveNewlines) {
  EXPECT_EQ(u"ab",
            WindowMetadataController::FormatTitleForDisplay(u"a\n\n\nb"));
}

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_LeadingNewline) {
  EXPECT_EQ(u"Hello",
            WindowMetadataController::FormatTitleForDisplay(u"\nHello"));
}

TEST_F(WindowMetadataControllerTest, FormatTitleForDisplay_TrailingNewline) {
  EXPECT_EQ(u"Hello",
            WindowMetadataController::FormatTitleForDisplay(u"Hello\n"));
}
