// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/tokenized_string.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

std::u16string GetContent(const TokenizedString& tokenized) {
  const TokenizedString::Tokens& tokens = tokenized.tokens();
  const TokenizedString::Mappings& mappings = tokenized.mappings();

  std::u16string str;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0)
      str += ' ';
    str += tokens[i];
    str += base::UTF8ToUTF16(mappings[i].ToString());
  }
  return str;
}

}  // namespace

TEST(TokenizedStringTest, Empty) {
  std::u16string empty;
  TokenizedString tokens(empty);
  EXPECT_EQ(std::u16string(), GetContent(tokens));
  TokenizedString token_words(empty, TokenizedString::Mode::kWords);
  EXPECT_EQ(std::u16string(), GetContent(token_words));
}

TEST(TokenizedStringTest, Basic) {
  {
    std::u16string text(u"a");
    TokenizedString tokens(text);
    EXPECT_EQ(u"a{0,1}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"a{0,1}", GetContent(token_words));
  }
  {
    std::u16string text(u"ScratchPad");
    TokenizedString tokens(text);
    EXPECT_EQ(u"scratch{0,7} pad{7,10}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"scratchpad{0,10}", GetContent(token_words));
  }
  {
    std::u16string text(u"Chess2.0");
    TokenizedString tokens(text);
    EXPECT_EQ(u"chess{0,5} 2.0{5,8}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"chess2.0{0,8}", GetContent(token_words));
  }
  {
    std::u16string text(u"Cut the rope");
    TokenizedString tokens(text);
    EXPECT_EQ(u"cut{0,3} the{4,7} rope{8,12}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"cut{0,3} the{4,7} rope{8,12}", GetContent(token_words));
  }
  {
    std::u16string text(u"AutoCAD WS");
    TokenizedString tokens(text);
    EXPECT_EQ(u"auto{0,4} cad{4,7} ws{8,10}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"autocad{0,7} ws{8,10}", GetContent(token_words));
  }
  {
    std::u16string text(u"Great TweetDeck");
    TokenizedString tokens(text);
    EXPECT_EQ(u"great{0,5} tweet{6,11} deck{11,15}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"great{0,5} tweetdeck{6,15}", GetContent(token_words));
  }
  {
    std::u16string text(u"Draw-It!");
    TokenizedString tokens(text);
    EXPECT_EQ(u"draw{0,4} it{5,7}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"draw-it{0,7}", GetContent(token_words));
  }
  {
    std::u16string text(u"Faxing & Signing");
    TokenizedString tokens(text);
    EXPECT_EQ(u"faxing{0,6} signing{9,16}", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"faxing{0,6} signing{9,16}", GetContent(token_words));
  }
  {
    std::u16string text(u"!@#$%^&*()<<<**>>>");
    TokenizedString tokens(text);
    EXPECT_EQ(u"", GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"", GetContent(token_words));
  }
}

TEST(TokenizedStringTest, TokenizeWords) {
  {
    std::u16string text(u"?! wi-fi abc@gmail.com?!");
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"wi-fi{3,8} abc@gmail.com{9,22}", GetContent(token_words));
  }
  {
    std::u16string text(u"Hello?! \t \b   World! ");
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"hello{0,5} world{14,19}", GetContent(token_words));
  }
  {
    std::u16string text(u" ?|! *&");
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(u"", GetContent(token_words));
  }
}

}  // namespace ash::string_matching
