// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_ui_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(IdentityUiUtilsTest, GetInitialLetterAsUppercase) {
  EXPECT_EQ(GetInitialLetterAsUppercase("😊 starts with an emoji"), u"😊");
  EXPECT_EQ(GetInitialLetterAsUppercase("English Text"), u"E");
  EXPECT_EQ(GetInitialLetterAsUppercase("النص العربي"), u"ا");
  EXPECT_EQ(GetInitialLetterAsUppercase("טקסט בעברית"), u"ט");
  EXPECT_EQ(GetInitialLetterAsUppercase("中文文本"), u"中");
  EXPECT_EQ(GetInitialLetterAsUppercase("h́ Text with combining character"),
            u"H́");
  EXPECT_EQ(GetInitialLetterAsUppercase(
                "👩🏾‍⚕️ Emoji with skin tone (combining character)"),
            u"👩🏾‍⚕️");
}
