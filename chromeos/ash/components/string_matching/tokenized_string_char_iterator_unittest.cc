// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/tokenized_string_char_iterator.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

// Returns a string represents the current state of |iter|. The state string
// has three fields. The first is the current char. The second is the offset of
// the current char in terms of the original text of the TokenizedString. The
// last one is optional and only shows up when IsFirstCharOfToken returns true.
std::string GetIterateState(const TokenizedStringCharIterator& iter) {
  return base::StringPrintf(
      "%s%d%s", base::UTF16ToUTF8(std::u16string(1, iter.Get())).c_str(),
      iter.GetArrayPos(), iter.IsFirstCharOfToken() ? "!" : "");
}

void TestBeyondTheEnd(TokenizedStringCharIterator* iter) {
  ASSERT_TRUE(iter->end());
  ASSERT_FALSE(iter->NextChar());
  ASSERT_FALSE(iter->NextToken());

  // Don't care what it returns, but this shouldn't crash.
  iter->Get();
}

void TestEveryChar(const std::string& text, const std::string& expects) {
  TokenizedString tokens(base::UTF8ToUTF16(text));
  TokenizedStringCharIterator iter(tokens);

  std::vector<std::string> results;
  while (!iter.end()) {
    results.push_back(GetIterateState(iter));
    iter.NextChar();
  }

  EXPECT_EQ(expects, base::JoinString(results, " "));
  TestBeyondTheEnd(&iter);
}

void TestNextToken(const std::string& text, const std::string& expects) {
  TokenizedString tokens(base::UTF8ToUTF16(text));
  TokenizedStringCharIterator iter(tokens);

  std::vector<std::string> results;
  while (!iter.end()) {
    results.push_back(GetIterateState(iter));
    iter.NextToken();
  }

  EXPECT_EQ(expects, base::JoinString(results, " "));
  TestBeyondTheEnd(&iter);
}

void TestFirstTwoCharInEveryToken(const std::string& text,
                                  const std::string& expects) {
  TokenizedString tokens(base::UTF8ToUTF16(text));
  TokenizedStringCharIterator iter(tokens);

  std::vector<std::string> results;
  while (!iter.end()) {
    results.push_back(GetIterateState(iter));
    if (iter.NextChar())
      results.push_back(GetIterateState(iter));

    iter.NextToken();
  }

  EXPECT_EQ(expects, base::JoinString(results, " "));
  TestBeyondTheEnd(&iter);
}

}  // namespace

TEST(TokenizedStringCharIteratorTest, NoTerms) {
  const char* text;

  text = "";
  TestEveryChar(text, "");
  TestNextToken(text, "");
  TestFirstTwoCharInEveryToken(text, "");

  text = "!@#$%^&*()<<<**>>>";
  TestEveryChar(text, "");
  TestNextToken(text, "");
  TestFirstTwoCharInEveryToken(text, "");
}

TEST(TokenizedStringCharIteratorTest, Basic) {
  const char* text;

  text = "c";
  TestEveryChar(text, "c0!");
  TestNextToken(text, "c0!");
  TestFirstTwoCharInEveryToken(text, "c0!");

  text = "Simple";
  TestEveryChar(text, "s0! i1 m2 p3 l4 e5");
  TestNextToken(text, "s0!");
  TestFirstTwoCharInEveryToken(text, "s0! i1");

  text = "ScratchPad";
  TestEveryChar(text, "s0! c1 r2 a3 t4 c5 h6 p7! a8 d9");
  TestNextToken(text, "s0! p7!");
  TestFirstTwoCharInEveryToken(text, "s0! c1 p7! a8");

  text = "Chess2.0";
  TestEveryChar(text, "c0! h1 e2 s3 s4 25! .6 07");
  TestNextToken(text, "c0! 25!");
  TestFirstTwoCharInEveryToken(text, "c0! h1 25! .6");

  text = "Cut the rope";
  TestEveryChar(text, "c0! u1 t2 t4! h5 e6 r8! o9 p10 e11");
  TestNextToken(text, "c0! t4! r8!");
  TestFirstTwoCharInEveryToken(text, "c0! u1 t4! h5 r8! o9");

  text = "AutoCAD WS";
  TestEveryChar(text, "a0! u1 t2 o3 c4! a5 d6 w8! s9");
  TestNextToken(text, "a0! c4! w8!");
  TestFirstTwoCharInEveryToken(text, "a0! u1 c4! a5 w8! s9");

  text = "Great TweetDeck";
  TestEveryChar(text, "g0! r1 e2 a3 t4 t6! w7 e8 e9 t10 d11! e12 c13 k14");
  TestNextToken(text, "g0! t6! d11!");
  TestFirstTwoCharInEveryToken(text, "g0! r1 t6! w7 d11! e12");

  text = "Draw-It!";
  TestEveryChar(text, "d0! r1 a2 w3 i5! t6");
  TestNextToken(text, "d0! i5!");
  TestFirstTwoCharInEveryToken(text, "d0! r1 i5! t6");

  text = "Faxing & Signing";
  TestEveryChar(text, "f0! a1 x2 i3 n4 g5 s9! i10 g11 n12 i13 n14 g15");
  TestNextToken(text, "f0! s9!");
  TestFirstTwoCharInEveryToken(text, "f0! a1 s9! i10");
}

}  // namespace ash::string_matching
