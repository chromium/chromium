// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TailoredWordBreakIterator, BreakWord) {
  std::u16string str(u"_foo_bar!_\npouet_boom");
  TailoredWordBreakIterator iter(str, TailoredWordBreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"foo", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"bar", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"!", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"pouet", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"boom", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(TailoredWordBreakIterator, TrailingUnderscore) {
  std::u16string str(u"_foo_bar_");
  TailoredWordBreakIterator iter(str, TailoredWordBreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"foo", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"bar", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"_", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}
