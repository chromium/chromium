// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_view_base.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webid {

TEST(AccountSelectionViewBaseTest, GetInitialLetterAsUppercase) {
  EXPECT_EQ(AccountSelectionViewBase::GetInitialLetterAsUppercase(
                "😊 starts with an emoji"),
            u"😊");
  EXPECT_EQ(
      AccountSelectionViewBase::GetInitialLetterAsUppercase("English Text"),
      u"E");
  EXPECT_EQ(
      AccountSelectionViewBase::GetInitialLetterAsUppercase("النص العربي"),
      u"ا");
  EXPECT_EQ(
      AccountSelectionViewBase::GetInitialLetterAsUppercase("טקסט בעברית"),
      u"ט");
  EXPECT_EQ(AccountSelectionViewBase::GetInitialLetterAsUppercase("中文文本"),
            u"中");
}

}  // namespace webid
