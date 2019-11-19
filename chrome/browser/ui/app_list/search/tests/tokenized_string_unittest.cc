// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/tokenized_string.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

namespace {

base::string16 GetContent(const ash::TokenizedString& tokenized) {
  const ash::TokenizedString::Tokens& tokens = tokenized.tokens();
  const ash::TokenizedString::Mappings& mappings = tokenized.mappings();

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
  ash::TokenizedString tokens(empty);
  EXPECT_EQ(base::string16(), GetContent(tokens));
}

TEST(TokenizedStringTest, Basic) {
  {
    base::string16 text(base::UTF8ToUTF16("ScratchPad"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("scratch{0,7} pad{7,10}"), GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Chess2.0"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("chess{0,5} 2.0{5,8}"), GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Cut the rope"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("cut{0,3} the{4,7} rope{8,12}"),
              GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("AutoCAD WS"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("auto{0,4} cad{4,7} ws{8,10}"),
              GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Great TweetDeck"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("great{0,5} tweet{6,11} deck{11,15}"),
              GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Draw-It!"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("draw{0,4} it{5,7}"), GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("Faxing & Signing"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16("faxing{0,6} signing{9,16}"),
              GetContent(tokens));
  }
  {
    base::string16 text(base::UTF8ToUTF16("!@#$%^&*()<<<**>>>"));
    ash::TokenizedString tokens(text);
    EXPECT_EQ(base::UTF8ToUTF16(""), GetContent(tokens));
  }
}

}  // namespace test
}  // namespace app_list
