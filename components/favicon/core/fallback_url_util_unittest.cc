// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/favicon/core/fallback_url_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace favicon {
namespace {

TEST(FallbackURLUtilTest, GetFallbackIconText) {
  struct {
    const char* url_str;
    const char* expected;
  } test_cases[] = {
    // Test vacuous or invalid cases.
    {"", ""},
    {"http:///", ""},
    {"this is not an URL", ""},
    {"!@#$%^&*()", ""},
    // Test URLs with a domain in the registry.
    {"http://www.google.com/", "G"},
    {"ftp://GOogLE.com/", "G"},
    {"https://www.google.com:8080/path?query#ref", "G"},
    {"http://www.amazon.com", "A"},
    {"http://zmzaon.co.uk/", "Z"},
    {"http://w-3.137.org", "1"},
    // Test URLs with a domain not in the registry.
    {"http://localhost/", "L"},
    {"chrome-search://most-visited/title.html", "M"},
    // Test IP URLs.
    {"http://192.168.0.1/", "IP"},
    {"http://[2001:4860:4860::8888]/", "IP"},
#if BUILDFLAG(IS_IOS)
    // Test Android app URLs.
    {"android://abc@org.coursera.android//", "A"},
#endif
    // Miscellaneous edge cases.
    {"http://www..com/", "."},
    {"http://ip.ip/", "I"},
    // xn-- related cases: we're not supporint xn-- yet
    {"http://xn--oogle-60a/", "X"},
    {"http://xn-oogle-60a/", "X"},
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::u16string expected = base::ASCIIToUTF16(test_cases[i].expected);
    GURL url(test_cases[i].url_str);
    EXPECT_EQ(expected, GetFallbackIconText(url)) << " for test_cases[" << i
                                                  << "]";
  }
}

}  // namespace
}  // namespace favicon
