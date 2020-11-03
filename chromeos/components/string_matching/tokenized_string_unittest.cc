// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/string_matching/tokenized_string.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace string_matching {

namespace {

base::string16 GetContent(const TokenizedString& tokenized) {
  const TokenizedString::Tokens& tokens = tokenized.tokens();
  const TokenizedString::Mappings& mappings = tokenized.mappings();

  base::string16 str;
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
  base::string16 empty;
  TokenizedString tokens(empty);
  EXPECT_EQ(base::string16(), GetContent(tokens));
  TokenizedString token_words(empty, TokenizedString::Mode::kWords);
  EXPECT_EQ(base::string16(), GetContent(token_words));
}

TEST(TokenizedStringTest, Basic) {
  {
    base::string16 text(base::UTF8ToUTF16("a"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("a{0,1}"), GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("a{0,1}"), GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("ScratchPad"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("scratch{0,7} pad{7,10}"), GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("scratchpad{0,10}"), GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Chess2.0"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("chess{0,5} 2.0{5,8}"), GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("chess2.0{0,8}"), GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Cut the rope"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("cut{0,3} the{4,7} rope{8,12}"),
              GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("cut{0,3} the{4,7} rope{8,12}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("AutoCAD WS"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("auto{0,4} cad{4,7} ws{8,10}"),
              GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("autocad{0,7} ws{8,10}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Great TweetDeck"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("great{0,5} tweet{6,11} deck{11,15}"),
              GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("great{0,5} tweetdeck{6,15}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Draw-It!"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("draw{0,4} it{5,7}"), GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("draw-it{0,7}"), GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Faxing & Signing"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("faxing{0,6} signing{9,16}"),
              GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("faxing{0,6} signing{9,16}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("!@#$%^&*()<<<**>>>"));
    TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16(""), GetContent(tokens));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16(""), GetContent(token_words));
  }
}

TEST(TokenizedStringTest, TokenizeWords) {
  {
    base::string16 text(base::UTF8ToUTF16("?! wi-fi abc@gmail.com?!"));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("wi-fi{3,8} abc@gmail.com{9,22}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Hello?! \t \b   World! "));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16("hello{0,5} world{14,19}"),
              GetContent(token_words));
  }
  {
    base::string16 text(base::UTF8ToUTF16(" ?|! *&"));
    TokenizedString token_words(text, TokenizedString::Mode::kWords);
    EXPECT_EQ(base::UTF8ToUTF16(""), GetContent(token_words));
  }
}

}  // namespace string_matching
}  // namespace chromeos
