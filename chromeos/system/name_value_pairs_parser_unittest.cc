// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/name_value_pairs_parser.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system {

TEST(NameValuePairsParser, TestParseNameValuePairs) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  const std::string contents1 = "foo=Foo bar=Bar\nfoobar=FooBar\n";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents1, "=", " \n", ""));
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);
  EXPECT_EQ("FooBar", map["foobar"]);

  map.clear();
  const std::string contents2 = "foo=Foo,bar=Bar";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents2, "=", ",\n", ""));
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);

  map.clear();
  const std::string contents3 = "foo=Foo=foo,bar=Bar";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents3, "=", ",\n", ""));
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ("Foo=foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);

  map.clear();
  const std::string contents4 = "foo=Foo,=Bar";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents4, "=", ",\n", ""));
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ("Foo", map["foo"]);

  map.clear();
  const std::string contents5 =
      "\"initial_locale\"=\"ja\"\n"
      "\"initial_timezone\"=\"Asia/Tokyo\"\n"
      "\"keyboard_layout\"=\"mozc-jp\"\n";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents5, "=", "\n", ""));
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ("ja", map["initial_locale"]);
  EXPECT_EQ("Asia/Tokyo", map["initial_timezone"]);
  EXPECT_EQ("mozc-jp", map["keyboard_layout"]);
}

TEST(NameValuePairsParser, TestParseNameValuePairsWithComments) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);

  const std::string contents1 = "foo=Foo,bar=#Bar,baz= 0 #Baz";
  EXPECT_TRUE(
      parser.ParseNameValuePairsWithComments(contents1, "=", ",\n", "#", ""));
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("", map["bar"]);
  EXPECT_EQ("0", map["baz"]);

  map.clear();
  const std::string contents2 = "foo=";
  EXPECT_TRUE(
      parser.ParseNameValuePairsWithComments(contents2, "=", ",\n", "#", ""));
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ("", map["foo"]);

  map.clear();
  const std::string contents3 = " \t ,,#all empty,";
  EXPECT_FALSE(
      parser.ParseNameValuePairsWithComments(contents3, "=", ",\n", "#", ""));
  EXPECT_EQ(0U, map.size());
}

TEST(NameValuePairsParser, TestParseNameValuePairsFromTool) {
  // Sample output is taken from the /usr/bin/crosssytem tool.
  const char* command[] = { "/bin/echo",
    "arch                   = x86           # Platform architecture\n" \
    "cros_debug             = 1             # OS should allow debug\n" \
    "dbg_reset              = (error)       # Debug reset mode request\n" \
    "key#with_comment       = some value    # Multiple # comment # delims\n" \
    "key                    =               # No value.\n" \
    "vdat_timers            = " \
        "LFS=0,0 LF=1784220250,2971030570 LK=9064076660,9342689170 " \
        "# Timer values from VbSharedData\n"
  };

  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  parser.ParseNameValuePairsFromTool(base::size(command), command, "=", "\n",
                                     "#");
  EXPECT_EQ(6u, map.size());
  EXPECT_EQ("x86", map["arch"]);
  EXPECT_EQ("1", map["cros_debug"]);
  EXPECT_EQ("(error)", map["dbg_reset"]);
  EXPECT_EQ("some value", map["key#with_comment"]);
  EXPECT_EQ("", map["key"]);
  EXPECT_EQ("LFS=0,0 LF=1784220250,2971030570 LK=9064076660,9342689170",
            map["vdat_timers"]);
}

TEST(NameValuePairsParser, DeletePairsWithValue) {
  NameValuePairsParser::NameValueMap map = {
      {"foo", "good"}, {"bar", "bad"}, {"baz", "good"}, {"end", "bad"},
  };
  NameValuePairsParser parser(&map);
  parser.DeletePairsWithValue("bad");
  ASSERT_EQ(2u, map.size());
  EXPECT_EQ("good", map["foo"]);
  EXPECT_EQ("good", map["baz"]);
}

}  // namespace system
}  // namespace chromeos
