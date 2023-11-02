// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/strings/string_util.h"

#include <string>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const wchar_t kEmptyStr[] = L"";
const wchar_t kFooStr[] = L"Foo";
const wchar_t kFooLowerCaseStr[] = L"foo";
const wchar_t kBarStr[] = L"Bar";
const wchar_t kBatStr[] = L"Bat";
const wchar_t kFooSetStr[] = L"Set,with,Foo,and,bar";
const wchar_t kFoooSetStr[] = L"Set,with,Fooo,and,bar";
const wchar_t kSeparators[] = L",";
const char kSomeInvalidUTF8Chars[] = " a\x80 b\x80 c ";
const char kStartWithInvalidUTF8Char[] = "\x80 a b c ";
const char kEndWithInvalidUTF8Chars[] = " a b c\x80 ";
const char kPrunedOfUTF8Chars[] = " a b c ";
const char kOnlyInvalidUTF8Chars[] = "\x80 \x80 ";
const char kNoCharsLeftOnlySpaces[] = "  ";
const char kSingleInvalidUTF8Char[] = "\xf1";

bool WildcardMatchInsensitive(const std::wstring& text,
                              const std::wstring& pattern) {
  return WStringWildcardMatchInsensitive(text, pattern, L'\\');
}

}  // namespace

TEST(StringUtilTest, WStringEqualsCaseInsensitive) {
  EXPECT_FALSE(WStringEqualsCaseInsensitive(kFooStr, kEmptyStr));
  EXPECT_TRUE(WStringEqualsCaseInsensitive(kFooStr, kFooStr));
  EXPECT_TRUE(WStringEqualsCaseInsensitive(kFooStr, kFooLowerCaseStr));
  EXPECT_FALSE(WStringEqualsCaseInsensitive(kFooStr, kBarStr));
  EXPECT_FALSE(WStringEqualsCaseInsensitive(kFooStr, kFooSetStr));
}

TEST(StringUtilTest, WStringContainsCaseInsensitive) {
  EXPECT_TRUE(WStringContainsCaseInsensitive(kFooStr, kEmptyStr));
  EXPECT_FALSE(WStringContainsCaseInsensitive(kEmptyStr, kFooStr));
  EXPECT_TRUE(WStringContainsCaseInsensitive(kFooStr, kFooStr));
  EXPECT_TRUE(WStringContainsCaseInsensitive(kFooLowerCaseStr, kFooStr));
  EXPECT_FALSE(WStringContainsCaseInsensitive(kBarStr, kFooStr));
  EXPECT_TRUE(WStringContainsCaseInsensitive(kFooSetStr, kFooStr));
  EXPECT_TRUE(WStringContainsCaseInsensitive(kFoooSetStr, kFooStr));
}

TEST(StringUtilTest, WStringSetMatchEntry) {
  EXPECT_TRUE(WStringSetMatchEntry(kFooSetStr, kSeparators, kFooStr,
                                   WStringContainsCaseInsensitive));
  EXPECT_FALSE(WStringSetMatchEntry(kFooSetStr, kSeparators, kBatStr,
                                    WStringContainsCaseInsensitive));
  EXPECT_TRUE(WStringSetMatchEntry(kFoooSetStr, kSeparators, kFooStr,
                                   WStringContainsCaseInsensitive));

  EXPECT_TRUE(WStringSetMatchEntry(kFooSetStr, kSeparators, kFooStr,
                                   WStringEqualsCaseInsensitive));
  EXPECT_FALSE(WStringSetMatchEntry(kFooSetStr, kSeparators, kBatStr,
                                    WStringEqualsCaseInsensitive));
  EXPECT_FALSE(WStringSetMatchEntry(kFoooSetStr, kSeparators, kFooStr,
                                    WStringEqualsCaseInsensitive));
}

TEST(StringUtilTest, WStringMatchPatternTest) {
  // Test matching on an empty text or pattern.
  EXPECT_TRUE(WildcardMatchInsensitive(L"", L""));
  EXPECT_FALSE(WildcardMatchInsensitive(L"", L"*.*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"", L"*"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"", L"?"));

  // Test matching recursion ending.
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"a?"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"b?"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L""));
  EXPECT_FALSE(WildcardMatchInsensitive(L"ab", L"a"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"ab"));

  // Test wild-cards matching.
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"*.com"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"?*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"*?"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"**"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"www.google.com", L"www*.g*.org"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"Hello", L"H?l?o"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"www.google.com", L"http://*)"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"Hello", L""));
  EXPECT_TRUE(WildcardMatchInsensitive(L"Hello*", L"Hello*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"1234-5678-1234-5678",
                                       LR"(????-????-????-????)"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"1234-5678-1234-5678", L"*-*-*-*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"123456789012345678",
                                       LR"(?????????????????*)"));

  // Test the case insensitive comparison.
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"*.COM"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"WWW.*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"*.*.COM"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"www.google.com", L"WWW.*.*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"WWW.GooGLe.com", L"www.*.c?m"));

  // Test escape characters.
  EXPECT_TRUE(WildcardMatchInsensitive(L"*", L"\\*"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"\\*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"?", L"\\?"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"\\?"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"*?*", L"\\*\\?\\*"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"*x*", L"\\*\\?\\*"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"Hello*1234", L"He??o\\*1*"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"Hello*1234", L"He??o\\?1*"));

  EXPECT_TRUE(WStringWildcardMatchInsensitive(L":", L"::", L':'));
  EXPECT_FALSE(WStringWildcardMatchInsensitive(L"*", L"\\*", L':'));
  EXPECT_TRUE(WStringWildcardMatchInsensitive(L"*", L":*", L':'));
  EXPECT_FALSE(WStringWildcardMatchInsensitive(L"?", L"\\?", L':'));
  EXPECT_TRUE(WStringWildcardMatchInsensitive(L"?", L":?", L':'));

  EXPECT_TRUE(WStringWildcardMatchInsensitive(L"*", L"%*", L'%'));
  EXPECT_FALSE(WStringWildcardMatchInsensitive(L"a", L"%*", L'%'));
  EXPECT_TRUE(WStringWildcardMatchInsensitive(L"?", L"%?", L'%'));
  EXPECT_FALSE(WStringWildcardMatchInsensitive(L"a", L"%?", L'%'));
  EXPECT_TRUE(WStringWildcardMatchInsensitive(L"*?*", L"%*%?%*", L'%'));
  EXPECT_FALSE(WStringWildcardMatchInsensitive(L"*x*", L"%*%?%*", L'%'));
  EXPECT_TRUE(
      WStringWildcardMatchInsensitive(L"Hello*1234", L"He??o%*1*", L'%'));
  EXPECT_FALSE(
      WStringWildcardMatchInsensitive(L"Hello*1234", L"He??o%?1*", L'%'));

  // Test the algorithmic complexity.
  EXPECT_TRUE(WildcardMatchInsensitive(L"", L"********************"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"a", L"**********?*********"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"ab", L"****?********?******"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"axb", L"****?****x***?******"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"^axb$", L"^****?****x***?******$"));

  EXPECT_FALSE(
      WildcardMatchInsensitive(L"a", L"x******************************"));
  EXPECT_FALSE(
      WildcardMatchInsensitive(L"a", L"******************************x"));
  EXPECT_TRUE(
      WildcardMatchInsensitive(L"a", L"?******************************"));
  EXPECT_TRUE(
      WildcardMatchInsensitive(L"a", L"******************************?"));
  EXPECT_FALSE(WildcardMatchInsensitive(
      L"1234", L"1********2********3********4********x"));
  EXPECT_FALSE(
      WildcardMatchInsensitive(L"1234567890", L"1*2*3*4*5*6*7*8*9*0x"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"1234567890", L"1*2*3*4*5*6*7*8*9*0"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"aaaaaaaaaaaaaaaaaaaaaaaaaa",
                                        L"********************************b"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"aaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       L"********************************"));

  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"**********x*********"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"a", L"****a*****x*********"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"b", L"**********x*****b***"));
  EXPECT_FALSE(WildcardMatchInsensitive(L"ab", L"***a*****x*****b***"));
  EXPECT_TRUE(WildcardMatchInsensitive(L"axb", L"***a*****x*****b***"));
}

TEST(StringUtilTest, RemoveInvalidUTF8Chars) {
  ASSERT_FALSE(base::IsStringUTF8(kSomeInvalidUTF8Chars));
  ASSERT_FALSE(base::IsStringUTF8(kStartWithInvalidUTF8Char));
  ASSERT_FALSE(base::IsStringUTF8(kEndWithInvalidUTF8Chars));
  ASSERT_FALSE(base::IsStringUTF8(kOnlyInvalidUTF8Chars));
  ASSERT_FALSE(base::IsStringUTF8(kSingleInvalidUTF8Char));
  ASSERT_TRUE(base::IsStringUTF8(kPrunedOfUTF8Chars));
  ASSERT_TRUE(base::IsStringUTF8(kNoCharsLeftOnlySpaces));

  EXPECT_STRNE(kPrunedOfUTF8Chars, kSomeInvalidUTF8Chars);
  EXPECT_STRNE(kPrunedOfUTF8Chars, kEndWithInvalidUTF8Chars);
  EXPECT_STRNE(kPrunedOfUTF8Chars, kEndWithInvalidUTF8Chars);
  EXPECT_STRNE("", kSingleInvalidUTF8Char);
  EXPECT_STRNE(kNoCharsLeftOnlySpaces, kOnlyInvalidUTF8Chars);

  EXPECT_STREQ(kPrunedOfUTF8Chars,
               RemoveInvalidUTF8Chars(kSomeInvalidUTF8Chars).c_str());
  EXPECT_STREQ(kPrunedOfUTF8Chars,
               RemoveInvalidUTF8Chars(kStartWithInvalidUTF8Char).c_str());
  EXPECT_STREQ(kPrunedOfUTF8Chars,
               RemoveInvalidUTF8Chars(kEndWithInvalidUTF8Chars).c_str());
  EXPECT_STREQ(kNoCharsLeftOnlySpaces,
               RemoveInvalidUTF8Chars(kOnlyInvalidUTF8Chars).c_str());
  EXPECT_STREQ(kPrunedOfUTF8Chars,
               RemoveInvalidUTF8Chars(kPrunedOfUTF8Chars).c_str());
  EXPECT_STREQ(kNoCharsLeftOnlySpaces,
               RemoveInvalidUTF8Chars(kNoCharsLeftOnlySpaces).c_str());
  EXPECT_STREQ("", RemoveInvalidUTF8Chars(kSingleInvalidUTF8Char).c_str());
}

TEST(StringUtilTest, WStringInsensitiveLess) {
  WStringInsensitiveLess less;
  EXPECT_TRUE(less(L"a", L"b"));
  EXPECT_TRUE(less(L"A", L"b"));
  EXPECT_TRUE(less(L"a", L"B"));
  EXPECT_TRUE(less(L"A", L"B"));

  EXPECT_FALSE(less(L"b", L"a"));
  EXPECT_FALSE(less(L"B", L"a"));
  EXPECT_FALSE(less(L"b", L"A"));
  EXPECT_FALSE(less(L"B", L"A"));

  EXPECT_FALSE(less(L"a", L"a"));
  EXPECT_FALSE(less(L"a", L"A"));
  EXPECT_FALSE(less(L"A", L"a"));
  EXPECT_FALSE(less(L"A", L"A"));
}

TEST(StringUtilTest, WStringInsensitiveSet) {
  WStringCaseInsensitiveSet set = {L"a", L"B"};
  EXPECT_NE(set.find(L"a"), set.end());
  EXPECT_NE(set.find(L"A"), set.end());
  EXPECT_NE(set.find(L"b"), set.end());
  EXPECT_NE(set.find(L"B"), set.end());
  EXPECT_EQ(set.find(L"c"), set.end());
  EXPECT_EQ(set.find(L"C"), set.end());
  EXPECT_FALSE(set.insert(L"A").second);
  EXPECT_FALSE(set.insert(L"b").second);
  EXPECT_TRUE(set.insert(L"c").second);
}

}  // namespace chrome_cleaner
