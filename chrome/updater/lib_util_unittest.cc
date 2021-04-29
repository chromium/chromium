// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lib_util.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// Characters that must be percent-encoded.
TEST(UnescapeUrlComponentTest, ReservedCharacters) {
  std::map<base::StringPiece, base::StringPiece> reserved_characters = {
      {"!", "%21"}, {"#", "%23"}, {"$", "%24"}, {"%", "%25"}, {"&", "%26"},
      {"'", "%27"}, {"(", "%28"}, {")", "%29"}, {"*", "%2A"}, {"+", "%2B"},
      {",", "%2C"}, {"/", "%2F"}, {":", "%3A"}, {";", "%3B"}, {"=", "%3D"},
      {"?", "%3F"}, {"@", "%40"}, {"[", "%5B"}, {"]", "%5D"}};

  for (auto pair : reserved_characters) {
    EXPECT_EQ(pair.first, UnescapeURLComponent(pair.second));
  }

  for (auto pair : reserved_characters) {
    // If input contains a reserved character, just ignore it.
    std::string escaped_includes_reserved = std::string(pair.first) + "%20";
    std::string unescaped = std::string(pair.first) + " ";
    EXPECT_EQ(unescaped, UnescapeURLComponent(escaped_includes_reserved));
  }
}

TEST(UnescapeUrlComponentTest, CommonCharacters) {
  std::map<base::StringPiece, base::StringPiece> common_characters = {
      {"\n", "%0A"}, {"\r", "%0D"},   {" ", "%20"},       {"\"", "%22"},
      {"%", "%25"},  {"-", "%2D"},    {".", "%2E"},       {"<", "%3C"},
      {">", "%3E"},  {"\\", "%5C"},   {"^", "%5E"},       {"_", "%5F"},
      {"`", "%60"},  {"{", "%7B"},    {"|", "%7C"},       {"}", "%7D"},
      {"~", "%7E"},  {"£", "%C2%A3"}, {"円", "%E5%86%86"}};

  for (auto pair : common_characters) {
    EXPECT_EQ(pair.first, UnescapeURLComponent(pair.second));
  }
}

TEST(UnescapeUrlComponentTest, MixedEscapedAndUnicode) {
  EXPECT_EQ("££", UnescapeURLComponent("£%C2%A3"));
}

}  // namespace updater
