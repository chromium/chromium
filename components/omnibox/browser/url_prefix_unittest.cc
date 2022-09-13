// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_prefix.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
struct TestCase {
  TestCase(const char* text,
           const char* prefix_suffix,
           const char* expected_prefix)
      : text(base::ASCIIToUTF16(text)),
        prefix_suffix(base::ASCIIToUTF16(prefix_suffix)),
        expected_prefix(base::ASCIIToUTF16(expected_prefix)) {}
  std::u16string text;
  std::u16string prefix_suffix;
  std::u16string expected_prefix;
};
}  // namespace

TEST(URLPrefix, BestURLPrefix) {
  const TestCase test_cases[] = {
      // Lowercase test cases with empty prefix suffix.
      TestCase("https://www.yandex.ru", "", "https://www."),
      TestCase("http://www.yandex.ru", "", "http://www."),
      TestCase("ftp://www.yandex.ru", "", "ftp://www."),
      TestCase("https://yandex.ru", "", "https://"),
      TestCase("http://yandex.ru", "", "http://"),
      TestCase("ftp://yandex.ru", "", "ftp://"),

      // Mixed case test cases with empty prefix suffix.
      TestCase("HTTPS://www.yandex.ru", "", "https://www."),
      TestCase("http://WWW.yandex.ru", "", "http://www."),

      // Cases with non empty prefix suffix.
      TestCase("http://www.yandex.ru", "yan", "http://www."),
      TestCase("https://www.yandex.ru", "YaN", "https://www."),

      // Prefix suffix does not match.
      TestCase("https://www.yandex.ru", "index", ""),
  };

  for (const TestCase& test_case : test_cases) {
    const URLPrefix* prefix =
        URLPrefix::BestURLPrefix(test_case.text, test_case.prefix_suffix);
    if (test_case.expected_prefix.empty()) {
      EXPECT_FALSE(prefix);
    } else {
      ASSERT_TRUE(prefix);
      EXPECT_EQ(test_case.expected_prefix, prefix->prefix);
    }
  }
}
