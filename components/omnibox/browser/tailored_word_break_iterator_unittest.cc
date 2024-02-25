// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
struct Piece {
  std::u16string string;
  bool is_word;
};

void VerifyBreaks(std::u16string str, std::vector<Piece> expected_pieces) {
  TailoredWordBreakIterator iter(str);
  ASSERT_TRUE(iter.Init());

  for (size_t i = 0; i < expected_pieces.size(); ++i) {
    ASSERT_TRUE(iter.Advance()) << base::StringPrintf(
        "Expected %zu pieces; found %zu pieces.\n", expected_pieces.size(), i);
    EXPECT_TRUE(iter.IsWord() == expected_pieces[i].is_word &&
                iter.GetString() == expected_pieces[i].string)
        << base::StringPrintf(
               "Expected {%s, %d}; found {%s, %d}.\n",
               base::UTF16ToUTF8(expected_pieces[i].string).c_str(),
               expected_pieces[i].is_word,
               base::UTF16ToUTF8(iter.GetString()).c_str(), iter.IsWord());
  }

  iter.Advance();
  ASSERT_EQ(iter.pos(), base::i18n::BreakIterator::npos) << base::StringPrintf(
      "Expected %zu pieces; found more pieces; found {%s, %d}.\n",
      expected_pieces.size(), base::UTF16ToUTF8(iter.GetString()).c_str(),
      iter.IsWord());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}
}  // namespace

TEST(TailoredWordBreakIterator, BreakWord) {
  VerifyBreaks(u"_foo_bar!_\npouet_boom", {
                                              {u"_", false},
                                              {u"foo", true},
                                              {u"_", false},
                                              {u"bar", true},
                                              {u"!", false},
                                              {u"_", false},
                                              {u"\n", false},
                                              {u"pouet", true},
                                              {u"_", false},
                                              {u"boom", true},
                                          });
}

TEST(TailoredWordBreakIterator, TrailingUnderscore) {
  VerifyBreaks(u"_foo_bar_", {
                                 {u"_", false},
                                 {u"foo", true},
                                 {u"_", false},
                                 {u"bar", true},
                                 {u"_", false},
                             });
}

TEST(TailoredWordBreakIterator, RepeatingUnderscore) {
  VerifyBreaks(u"Viktor...Ambartsumian", {
                                             {u"Viktor", true},
                                             {u".", false},
                                             {u".", false},
                                             {u".", false},
                                             {u"Ambartsumian", true},
                                         });

  VerifyBreaks(u"Viktor___Ambartsumian", {
                                             {u"Viktor", true},
                                             {u"_", false},
                                             {u"_", false},
                                             {u"_", false},
                                             {u"Ambartsumian", true},
                                         });

  VerifyBreaks(u"Viktor_..///.__Ambartsumian", {
                                                   {u"Viktor", true},
                                                   {u"_", false},
                                                   {u".", false},
                                                   {u".", false},
                                                   {u"/", false},
                                                   {u"/", false},
                                                   {u"/", false},
                                                   {u".", false},
                                                   {u"_", false},
                                                   {u"_", false},
                                                   {u"Ambartsumian", true},
                                               });
}

TEST(TailoredWordBreakIterator, Numerics) {
  VerifyBreaks(u"chr0m3 15 aw350m3", {
                                         {u"chr", true},
                                         {u"0", true},
                                         {u"m", true},
                                         {u"3", true},
                                         {u" ", false},
                                         {u"15", true},
                                         {u" ", false},
                                         {u"aw", true},
                                         {u"350", true},
                                         {u"m", true},
                                         {u"3", true},
                                     });
}

TEST(TailoredWordBreakIterator, NumericsAndUnderscores) {
  VerifyBreaks(u"chr0m3__15__aw350m3", {
                                           {u"chr", true},
                                           {u"0", true},
                                           {u"m", true},
                                           {u"3", true},
                                           {u"_", false},
                                           {u"_", false},
                                           {u"15", true},
                                           {u"_", false},
                                           {u"_", false},
                                           {u"aw", true},
                                           {u"350", true},
                                           {u"m", true},
                                           {u"3", true},
                                       });

  VerifyBreaks(u"Viktor Ambartsumian_is__anAwesome99_99Astrophysicist!!",
               {
                   {u"Viktor", true},
                   {u" ", false},
                   {u"Ambartsumian", true},
                   {u"_", false},
                   {u"is", true},
                   {u"_", false},
                   {u"_", false},
                   {u"anAwesome", true},
                   {u"99", true},
                   {u"_", false},
                   {u"99", true},
                   {u"Astrophysicist", true},
                   {u"!", false},
                   {u"!", false},
               });
}
