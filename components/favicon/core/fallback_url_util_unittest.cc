// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_url_util.h"

#include <stddef.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace favicon {
namespace {

TEST(FallbackURLUtilTest, GetFallbackIconText) {
  struct TestCases {
    const char* url_str;
    const char16_t* expected;
  };
  auto test_cases = std::to_array<TestCases>({
      // Test vacuous or invalid cases.
      {"", u""},
      {"http:///", u""},
      {"this is not an URL", u""},
      {"!@#$%^&*()", u""},
      // Test URLs with a domain in the registry.
      {"http://www.google.com/", u"G"},
      {"ftp://GOogLE.com/", u"G"},
      {"https://www.google.com:8080/path?query#ref", u"G"},
      {"http://www.amazon.com", u"A"},
      {"http://zmzaon.co.uk/", u"Z"},
      {"http://w-3.137.org", u"1"},
      // Test URLs with a domain not in the registry.
      {"http://localhost/", u"L"},
      {"chrome-search://most-visited/title.html", u"M"},
      // Test IP URLs.
      {"http://192.168.0.1/", u"IP"},
      {"http://[2001:4860:4860::8888]/", u"IP"},
#if BUILDFLAG(IS_IOS)
      // Test Android app URLs.
      {"android://abc@org.coursera.android//", u"A"},
#endif
      // Miscellaneous edge cases.
      {"http://www..com/", u"."},
      {"http://ip.ip/", u"I"},
      // Punycode cases.
      {"http://xn--mllerriis-l8a.dk/", u"M"},
      {"http://xn--hq1bm8jm9l.com/", u"도"},
      {"http://xn--22cdfh1b8fsa.com/", u"ย"},
      {"http://xn--h1acbxfam.com/", u"Р"},
      {"http://xn--mnchen-3ya.de/", u"M"},
  });
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::u16string expected = test_cases[i].expected;
    GURL url(test_cases[i].url_str);
    EXPECT_EQ(expected, GetFallbackIconText(url)) << " for test_cases[" << i
                                                  << "]";
  }
}

}  // namespace
}  // namespace favicon
