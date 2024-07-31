// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/url_formatter/url_formatter.h"

#include <stddef.h>
#include <string.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_formatter {

namespace {

using base::WideToUTF16;
using base::ASCIIToUTF16;

const size_t kNpos = std::u16string::npos;

struct AdjustOffsetCase {
  size_t input_offset;
  size_t output_offset;
};

struct UrlTestData {
  const char* const description;
  const char* const input;
  FormatUrlTypes format_types;
  base::UnescapeRule::Type escape_rules;
  const wchar_t* output;  // Use |wchar_t| to handle Unicode constants easily.
  size_t prefix_len;
};

// A pair of helpers for the FormatUrlWithOffsets() test.
void VerboseExpect(size_t expected,
                   size_t actual,
                   const std::string& original_url,
                   size_t position,
                   const std::u16string& formatted_url) {
  EXPECT_EQ(expected, actual) << "Original URL: " << original_url
      << " (at char " << position << ")\nFormatted URL: " << formatted_url;
}

void CheckAdjustedOffsets(const std::string& url_string,
                          FormatUrlTypes format_types,
                          base::UnescapeRule::Type unescape_rules,
                          const size_t* output_offsets) {
  GURL url(url_string);
  size_t url_length = url_string.length();
  std::vector<size_t> offsets;
  for (size_t i = 0; i <= url_length + 1; ++i)
    offsets.push_back(i);
  offsets.push_back(500000);  // Something larger than any input length.
  offsets.push_back(std::string::npos);
  std::u16string formatted_url = FormatUrlWithOffsets(
      url, format_types, unescape_rules, nullptr, nullptr, &offsets);
  for (size_t i = 0; i < url_length; ++i)
    VerboseExpect(output_offsets[i], offsets[i], url_string, i, formatted_url);
  VerboseExpect(formatted_url.length(), offsets[url_length], url_string,
                url_length, formatted_url);
  VerboseExpect(std::u16string::npos, offsets[url_length + 1], url_string,
                500000, formatted_url);
  VerboseExpect(std::u16string::npos, offsets[url_length + 2], url_string,
                std::string::npos, formatted_url);
}

}  // namespace

TEST(UrlFormatterTest, FormatUrl) {
  FormatUrlTypes default_format_type = kFormatUrlOmitUsernamePassword;
  // clang-format off
  const UrlTestData tests[] = {
      {"Empty URL", "", default_format_type, base::UnescapeRule::NORMAL, L"", 0},

      {"Simple URL", "http://www.google.com/", default_format_type,
       base::UnescapeRule::NORMAL, L"http://www.google.com/", 7},

      {"With a port number and a reference",
       "http://www.google.com:8080/#\xE3\x82\xB0", default_format_type,
       base::UnescapeRule::NORMAL, L"http://www.google.com:8080/#\x30B0", 7},

      // -------- IDN tests --------
      {"Japanese IDN with ja", "http://xn--l8jvb1ey91xtjb.jp",
       default_format_type, base::UnescapeRule::NORMAL,
       L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

      {"mailto: with Japanese IDN", "mailto:foo@xn--l8jvb1ey91xtjb.jp",
       default_format_type, base::UnescapeRule::NORMAL,
       // GURL doesn't assume an email address's domain part as a host name.
       L"mailto:foo@xn--l8jvb1ey91xtjb.jp", 7},

      {"file: with Japanese IDN", "file://xn--l8jvb1ey91xtjb.jp/config.sys",
       default_format_type, base::UnescapeRule::NORMAL,
       L"file://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 7},

      {"ftp: with Japanese IDN", "ftp://xn--l8jvb1ey91xtjb.jp/config.sys",
       default_format_type, base::UnescapeRule::NORMAL,
       L"ftp://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 6},

      // -------- omit_username_password flag tests --------
      {"With username and password, omit_username_password=false",
       "http://user:passwd@example.com/foo", kFormatUrlOmitNothing,
       base::UnescapeRule::NORMAL, L"http://user:passwd@example.com/foo", 19},

      {"With username and password, omit_username_password=true",
       "http://user:passwd@example.com/foo", default_format_type,
       base::UnescapeRule::NORMAL, L"http://example.com/foo", 7},

      {"With username and no password", "http://user@example.com/foo",
       default_format_type, base::UnescapeRule::NORMAL,
       L"http://example.com/foo", 7},

      {"Just '@' without username and password", "http://@example.com/foo",
       default_format_type, base::UnescapeRule::NORMAL,
       L"http://example.com/foo", 7},

      // GURL doesn't think local-part of an email address is username for URL.
      {"mailto:, omit_username_password=true", "mailto:foo@example.com",
       default_format_type, base::UnescapeRule::NORMAL,
       L"mailto:foo@example.com", 7},

      // -------- unescape flag tests --------
      {"Do not unescape",
       "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
       "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
       "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
       default_format_type, base::UnescapeRule::NONE,
       // GURL parses %-encoded hostnames into Punycode.
       L"http://\x30B0\x30FC\x30B0\x30EB.jp/"
       L"%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
       L"?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
       7},

      {"Unescape normally",
       "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
       "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
       "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
       default_format_type, base::UnescapeRule::NORMAL,
       L"http://\x30B0\x30FC\x30B0\x30EB.jp/\x30B0\x30FC\x30B0\x30EB"
       L"?q=\x30B0\x30FC\x30B0\x30EB",
       7},

      {"Unescape normally with BiDi control character",
       "http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", default_format_type,
       base::UnescapeRule::NORMAL,
       L"http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", 7},

      {"Unescape normally including unescape spaces",
       "http://www.google.com/search?q=Hello%20World", default_format_type,
       base::UnescapeRule::SPACES, L"http://www.google.com/search?q=Hello World",
       7},

      /*
      {"unescape=true with some special characters",
      "http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z",
      kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
      L"http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", 25},
      */
      // Disabled: the resultant URL becomes "...user%253A:%2540passwd...".

      // -------- omit http: --------
      {"omit http", "http://www.google.com/", kFormatUrlOmitHTTP,
       base::UnescapeRule::NORMAL, L"www.google.com/", 0},

      {"omit http on bare scheme", "http://", kFormatUrlOmitDefaults,
       base::UnescapeRule::NORMAL, L"", 0},

      {"omit http with user name", "http://user@example.com/foo",
       kFormatUrlOmitDefaults, base::UnescapeRule::NORMAL, L"example.com/foo",
       0},

      {"omit http with https", "https://www.google.com/", kFormatUrlOmitHTTP,
       base::UnescapeRule::NORMAL, L"https://www.google.com/", 8},

      {"omit http starts with ftp.", "http://ftp.google.com/",
       kFormatUrlOmitHTTP, base::UnescapeRule::NORMAL, L"http://ftp.google.com/",
       7},

      // -------- omit file: --------
#if BUILDFLAG(IS_WIN)
      {"omit file on Windows", "file:///C:/Users/homedirname/folder/file.pdf/",
       kFormatUrlOmitFileScheme, base::UnescapeRule::NORMAL,
       L"C:/Users/homedirname/folder/file.pdf/", static_cast<size_t>(-1)},
#else
      {"omit file", "file:///Users/homedirname/folder/file.pdf/",
       kFormatUrlOmitFileScheme, base::UnescapeRule::NORMAL,
       L"/Users/homedirname/folder/file.pdf/", 0},
#endif
      // -------- omit mailto: --------
      { "omit mailto", "mailto:foo@bar.com",
      kFormatUrlOmitMailToScheme, base::UnescapeRule::NORMAL,
      L"foo@bar.com", 0 },

      // -------- omit trailing slash on bare hostname --------
      {"omit slash when it's the entire path", "http://www.google.com/",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"http://www.google.com", 7},
      {"omit slash when there's a ref", "http://www.google.com/#ref",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"http://www.google.com/#ref", 7},
      {"omit slash when there's a query", "http://www.google.com/?",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"http://www.google.com/?", 7},
      {"omit slash when it's not the entire path", "http://www.google.com/foo",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"http://www.google.com/foo", 7},
      {"omit slash for nonstandard URLs", "data:/",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"data:/", 5},
      {"omit slash for file URLs", "file:///",
       kFormatUrlOmitTrailingSlashOnBareHostname, base::UnescapeRule::NORMAL,
       L"file:///", 7},

      // -------- view-source: --------
      {"view-source", "view-source:http://xn--qcka1pmc.jp/",
       default_format_type, base::UnescapeRule::NORMAL,
       L"view-source:http://\x30B0\x30FC\x30B0\x30EB.jp/", 19},

      {"view-source of view-source",
       "view-source:view-source:http://xn--qcka1pmc.jp/", default_format_type,
       base::UnescapeRule::NORMAL,
       L"view-source:view-source:http://xn--qcka1pmc.jp/", 12},

      // view-source should omit http and trailing slash where non-view-source
      // would.
      {"view-source omit http", "view-source:http://a.b/c",
       kFormatUrlOmitDefaults, base::UnescapeRule::NORMAL, L"view-source:a.b/c",
       12},
      {"view-source omit http starts with ftp.", "view-source:http://ftp.b/c",
       kFormatUrlOmitDefaults, base::UnescapeRule::NORMAL,
       L"view-source:http://ftp.b/c", 19},
      {"view-source omit slash when it's the entire path",
       "view-source:http://a.b/", kFormatUrlOmitDefaults,
       base::UnescapeRule::NORMAL, L"view-source:a.b", 12},
      {"view-source never applies destructive elisions to its inner URL",
       "view-source:https://www.google.com/foo",
       kFormatUrlOmitDefaults | kFormatUrlOmitHTTPS |
           kFormatUrlOmitTrivialSubdomains | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"view-source:https://www.google.com/foo",
       20},
#if BUILDFLAG(IS_WIN)
      {"view-source should not omit file on Windows",
       "view-source:file:///C:/Users/homedirname/folder/file.pdf/",
       kFormatUrlOmitDefaults | kFormatUrlOmitFileScheme,
       base::UnescapeRule::NORMAL,
       L"view-source:file:///C:/Users/homedirname/folder/file.pdf/", 19},
#else
      {"view-source should not omit file",
       "view-source:file:///Users/homedirname/folder/file.pdf/",
       kFormatUrlOmitDefaults | kFormatUrlOmitFileScheme,
       base::UnescapeRule::NORMAL,
       L"view-source:file:///Users/homedirname/folder/file.pdf/", 19},
#endif

      // -------- omit https --------
      {"omit https", "https://www.google.com/", kFormatUrlOmitHTTPS,
       base::UnescapeRule::NORMAL, L"www.google.com/", 0},
      {"omit https but do not omit http", "http://www.google.com/",
       kFormatUrlOmitHTTPS, base::UnescapeRule::NORMAL,
       L"http://www.google.com/", 7},
      {"omit https, username, and password",
       "https://user:password@example.com/foo",
       kFormatUrlOmitDefaults | kFormatUrlOmitHTTPS, base::UnescapeRule::NORMAL,
       L"example.com/foo", 0},
      {"omit https, but preserve user name and password",
       "https://user:password@example.com/foo", kFormatUrlOmitHTTPS,
       base::UnescapeRule::NORMAL, L"user:password@example.com/foo", 14},
      {"omit https should not affect hosts starting with ftp.",
       "https://ftp.google.com/", kFormatUrlOmitHTTP | kFormatUrlOmitHTTPS,
       base::UnescapeRule::NORMAL, L"https://ftp.google.com/", 8},

      // -------- omit trivial subdomains --------
      {"omit trivial subdomains - trim leading www",
      "http://www.wikipedia.org/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, L"http://wikipedia.org/", 7},
      {"omit trivial subdomains - don't trim leading m",
      "http://m.google.com/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, L"http://m.google.com/", 7},
      {"omit trivial subdomains - don't trim www after a leading m",
      "http://m.www.google.com/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, L"http://m.www.google.com/", 7},
      {"omit trivial subdomains - trim first www only",
      "http://www.www.www.wikipedia.org/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, L"http://www.www.wikipedia.org/", 7},
      {"omit trivial subdomains - don't trim www from middle",
      "http://en.www.wikipedia.org/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, L"http://en.www.wikipedia.org/", 7},
      {"omit trivial subdomains - don't do blind substring matches for www",
       "http://foowww.google.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://foowww.google.com/", 7},
      {"omit trivial subdomains - don't crash on multiple delimiters",
       "http://www....foobar...google.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://...foobar...google.com/", 7},

      {"omit trivial subdomains - sanity check for ordinary subdomains",
       "http://mail.yahoo.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://mail.yahoo.com/", 7},
      {"omit trivial subdomains - sanity check for auth",
       "http://www:m@google.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://www:m@google.com/", 13},
      {"omit trivial subdomains - sanity check for path",
       "http://google.com/www.m.foobar", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://google.com/www.m.foobar", 7},
      {"omit trivial subdomains - sanity check for IDN",
       "http://www.xn--cy2a840a.www.xn--cy2a840a.com",
       kFormatUrlOmitTrivialSubdomains, base::UnescapeRule::NORMAL,
       L"http://\x89c6\x9891.www.\x89c6\x9891.com/", 7},

      {"omit trivial subdomains but leave registry and domain alone - trivial",
       "http://google.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://google.com/", 7},
      {"omit trivial subdomains but leave registry and domain alone - www",
       "http://www.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://www.com/", 7},
      {"omit trivial subdomains but leave registry and domain alone - co.uk",
       "http://m.co.uk/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://m.co.uk/", 7},
      {"omit trivial subdomains but leave eTLD (effective TLD) alone",
       "http://www.appspot.com/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://www.appspot.com/", 7},


      {"omit trivial subdomains but leave intranet hostnames alone",
       "http://router/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://router/", 7},
      {"omit trivial subdomains but leave alone if host itself is a registry",
       "http://co.uk/", kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://co.uk/", 7},

#if BUILDFLAG(IS_IOS)
      // -------- omit mobile prefix --------
      {"omit mobile prefix - trim leading m.",
       "http://m.wikipedia.org/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://wikipedia.org/", 7},
      {"omit mobile prefix - trim leading m., but don't trim www",
       "http://m.www.google.com/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://www.google.com/", 7},
      {"omit mobile prefix - trim first m. only",
       "http://m.m.m.wikipedia.org/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://m.m.wikipedia.org/", 7},
      {"omit mobile prefix - don't trim m. from middle",
       "http://en.m.wikipedia.org/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://en.m.wikipedia.org/", 7},
      {"omit mobile prefix - don't do blind substring matches for m.",
       "http://foom.google.com/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://foom.google.com/", 7},
      {"omit mobile prefix - don't crash on multiple delimiters",
       "http://m....foobar...google.com/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://...foobar...google.com/", 7},
      {"omit mobile prefix - sanity check for ordinary subdomains",
       "http://mail.yahoo.com/", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://mail.yahoo.com/", 7},
      {"omit mobile prefix - sanity check for path",
       "http://google.com/www.m.foobar", kFormatUrlOmitMobilePrefix,
       base::UnescapeRule::NORMAL, L"http://google.com/www.m.foobar", 7},

      // -------- omit mobile prefix and trivial subdomains --------
      {"omit mobile prefix and trivial subdomains - trim leading m. and www.",
      "http://m.www.wikipedia.org/", kFormatUrlOmitMobilePrefix | kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://wikipedia.org/", 7},
      {"omit mobile prefix and trivial subdomains - trim leading m., "
       "but don't trim www", "http://m.wwwwikipedia.org/",
       kFormatUrlOmitMobilePrefix | kFormatUrlOmitTrivialSubdomains,
       base::UnescapeRule::NORMAL, L"http://wwwwikipedia.org/", 7},
      {"omit mobile prefix and trivial subdomains - trim leading www. and m.",
      "http://www.m.wikipedia.org/", kFormatUrlOmitMobilePrefix |
       kFormatUrlOmitTrivialSubdomains, base::UnescapeRule::NORMAL,
       L"http://wikipedia.org/", 7},
#endif

      // -------- trim after host --------
      {"omit the trailing slash when ommitting the path", "http://google.com/",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit the simple file path when ommitting the path",
       "http://google.com/foo",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit the file and folder path when ommitting the path",
       "http://google.com/ab/cd",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with query only",
       "http://google.com/?foo=bar",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with ref only", "http://google.com/#foobar",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with path and query only",
       "http://google.com/foo?a=b",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with path and ref only",
       "http://google.com/foo#c",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with query and ref only",
       "http://google.com/?a=b#c",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with path, query and ref",
       "http://google.com/foo?a=b#c",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"omit everything after host with repeated delimiters (sanity check)",
       "http://google.com////???####",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL, L"google.com", 0},
      {"never trim file paths", "file:///Users/homedirname/folder/file.pdf/",
       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
       base::UnescapeRule::NORMAL,
       L"file:///Users/homedirname/folder/file.pdf/", 7},
  };
  // clang-format on

  for (size_t i = 0; i < std::size(tests); ++i) {
    size_t prefix_len;
    std::u16string formatted =
        FormatUrl(GURL(tests[i].input), tests[i].format_types,
                  tests[i].escape_rules, nullptr, &prefix_len, nullptr);
    EXPECT_EQ(WideToUTF16(tests[i].output), formatted) << tests[i].description;
    EXPECT_EQ(tests[i].prefix_len, prefix_len) << tests[i].description;
  }
}

TEST(UrlFormatterTest, FormatUrlParsed) {
  // No unescape case.
  url::Parsed parsed;
  std::u16string formatted =
      FormatUrl(GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
                     "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
                kFormatUrlOmitNothing, base::UnescapeRule::NONE, &parsed,
                nullptr, nullptr);
  EXPECT_EQ(
      u"http://%E3%82%B0:%E3%83%BC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      u"/%E3%82%B0/?q=%E3%82%B0#%E3%82%B0",
      formatted);
  EXPECT_EQ(u"%E3%82%B0",
            formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(u"%E3%83%BC",
            formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(u"\x30B0\x30FC\x30B0\x30EB.jp",
            formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/%E3%82%B0/",
            formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"q=%E3%82%B0",
            formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"%E3%82%B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Unescape case.
  formatted =
      FormatUrl(GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
                     "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
                kFormatUrlOmitNothing, base::UnescapeRule::NORMAL, &parsed,
                nullptr, nullptr);
  EXPECT_EQ(
      u"http://\x30B0:\x30FC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      u"/\x30B0/?q=\x30B0#\x30B0",
      formatted);
  EXPECT_EQ(u"\x30B0",
            formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(u"\x30FC",
            formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(u"\x30B0\x30FC\x30B0\x30EB.jp",
            formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/\x30B0/", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"q=\x30B0",
            formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"\x30B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Omit_username_password + unescape case.
  formatted =
      FormatUrl(GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
                     "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
                kFormatUrlOmitUsernamePassword, base::UnescapeRule::NORMAL,
                &parsed, nullptr, nullptr);
  EXPECT_EQ(
      u"http://\x30B0\x30FC\x30B0\x30EB.jp:8080"
      u"/\x30B0/?q=\x30B0#\x30B0",
      formatted);
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(u"\x30B0\x30FC\x30B0\x30EB.jp",
            formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/\x30B0/", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"q=\x30B0",
            formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"\x30B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // View-source case.
  formatted =
      FormatUrl(GURL("view-source:http://user:passwd@host:81/path?query#ref"),
                kFormatUrlOmitUsernamePassword, base::UnescapeRule::NORMAL,
                &parsed, nullptr, nullptr);
  EXPECT_EQ(u"view-source:http://host:81/path?query#ref", formatted);
  EXPECT_EQ(u"view-source:http",
            formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(u"host", formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"81", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/path", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"query", formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"ref", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Repeated view-source separated by a space.
  formatted = FormatUrl(
      GURL(
          "view-source: view-source:http://user:passwd@host:81/path?query#ref"),
      kFormatUrlOmitUsernamePassword, base::UnescapeRule::NORMAL, &parsed,
      nullptr, nullptr);
  EXPECT_EQ(
      u"view-source: view-source:http://user:passwd@host:81/path?query#ref",
      formatted);
  EXPECT_EQ(u"view-source",
            formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_FALSE(parsed.host.is_valid());
  EXPECT_FALSE(parsed.port.is_valid());
  EXPECT_EQ(u" view-source:http://user:passwd@host:81/path",
            formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"query", formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"ref", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http case.
  formatted = FormatUrl(GURL("http://host:8000/a?b=c#d"), kFormatUrlOmitHTTP,
                        base::UnescapeRule::NORMAL, &parsed, nullptr, nullptr);
  EXPECT_EQ(u"host:8000/a?b=c#d", formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(u"host", formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"8000", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/a", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"b=c", formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"d", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with ftp case.
  formatted =
      FormatUrl(GURL("http://ftp.host:8000/a?b=c#d"), kFormatUrlOmitHTTP,
                base::UnescapeRule::NORMAL, &parsed, nullptr, nullptr);
  EXPECT_EQ(u"http://ftp.host:8000/a?b=c#d", formatted);
  EXPECT_TRUE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(u"http", formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_EQ(u"ftp.host", formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"8000", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(u"/a", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(u"b=c", formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(u"d", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with 'f' case.
  formatted = FormatUrl(GURL("http://f/"), kFormatUrlOmitHTTP,
                        base::UnescapeRule::NORMAL, &parsed, nullptr, nullptr);
  EXPECT_EQ(u"f/", formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_FALSE(parsed.port.is_valid());
  EXPECT_TRUE(parsed.path.is_valid());
  EXPECT_FALSE(parsed.query.is_valid());
  EXPECT_FALSE(parsed.ref.is_valid());
  EXPECT_EQ(u"f", formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(u"/", formatted.substr(parsed.path.begin, parsed.path.len));
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the path.
TEST(UrlFormatterTest, FormatUrlRoundTripPathASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/") +
             static_cast<char>(test_char));
    size_t prefix_len;
    std::u16string formatted =
        FormatUrl(url, kFormatUrlOmitUsernamePassword,
                  base::UnescapeRule::NORMAL, nullptr, &prefix_len, nullptr);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in a different GURL, for each escaped ASCII character in the path.
// GURL no longer unescapes percent-encoded ASCII characters. See
// https://crbug.com/1252531
TEST(UrlFormatterTest, FormatUrlRoundTripPathEscaped) {
  // A full list of characters which FormatURL should unescape.
  const std::string_view kUnescapedCharacters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_~";

  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    std::u16string formatted =
        FormatUrl(url, kFormatUrlOmitUsernamePassword,
                  base::UnescapeRule::NORMAL, nullptr, &prefix_len, nullptr);
    if (test_char && kUnescapedCharacters.find(static_cast<char>(test_char)) !=
                         kUnescapedCharacters.npos) {
      EXPECT_NE(url.spec(), GURL(formatted).spec());
    } else {
      EXPECT_EQ(url.spec(), GURL(formatted).spec());
    }
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the query.
TEST(UrlFormatterTest, FormatUrlRoundTripQueryASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/?") +
             static_cast<char>(test_char));
    size_t prefix_len;
    std::u16string formatted =
        FormatUrl(url, kFormatUrlOmitUsernamePassword,
                  base::UnescapeRule::NORMAL, nullptr, &prefix_len, nullptr);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// only results in a different GURL for certain characters.
TEST(UrlFormatterTest, FormatUrlRoundTripQueryEscaped) {
  // A full list of characters which FormatURL should unescape and GURL should
  // not escape again, when they appear in a query string.
  const char kUnescapedCharacters[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_~";
  for (unsigned char test_char = 0; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/?");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    std::u16string formatted =
        FormatUrl(url, kFormatUrlOmitUsernamePassword,
                  base::UnescapeRule::NORMAL, nullptr, &prefix_len, nullptr);

    if (test_char &&
        strchr(kUnescapedCharacters, static_cast<char>(test_char))) {
      EXPECT_NE(url.spec(), GURL(formatted).spec());
    } else {
      EXPECT_EQ(url.spec(), GURL(formatted).spec());
    }
  }
}

TEST(UrlFormatterTest, StripWWWFromHostComponent) {
  {
    // Typical public URL should have www stripped.
    std::string url = "https://www.google.com/abc";
    url::Component host(8, 14);
    ASSERT_EQ("www.google.com", url.substr(host.begin, host.len));
    StripWWWFromHostComponent(url, &host);
    EXPECT_EQ("google.com", url.substr(host.begin, host.len));
  }
  {
    // Intranet hostname should not have www stripped.
    std::string url = "https://www.foobar/abc";
    url::Component host(8, 10);
    ASSERT_EQ("www.foobar", url.substr(host.begin, host.len));
    StripWWWFromHostComponent(url, &host);
    EXPECT_EQ("www.foobar", url.substr(host.begin, host.len));
  }
  {
    // Domain and registry should be excluded from www stripping.
    std::string url = "https://www.co.uk/abc";
    url::Component host(8, 9);
    ASSERT_EQ("www.co.uk", url.substr(host.begin, host.len));
    StripWWWFromHostComponent(url, &host);
    EXPECT_EQ("www.co.uk", url.substr(host.begin, host.len));
  }
}

TEST(UrlFormatterTest, FormatUrlWithOffsets) {
  CheckAdjustedOffsets(std::string(), kFormatUrlOmitNothing,
                       base::UnescapeRule::NORMAL, nullptr);

  const size_t basic_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25
  };
  CheckAdjustedOffsets("http://www.google.com/foo/", kFormatUrlOmitNothing,
                       base::UnescapeRule::NORMAL, basic_offsets);

  const size_t omit_auth_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 7,
    8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo:bar@www.google.com/",
                       kFormatUrlOmitUsernamePassword,
                       base::UnescapeRule::NORMAL, omit_auth_offsets_1);

  const size_t omit_auth_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo@www.google.com/",
                       kFormatUrlOmitUsernamePassword,
                       base::UnescapeRule::NORMAL, omit_auth_offsets_2);

  const size_t dont_omit_auth_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31
  };
  // Unescape to "http://foo\x30B0:\x30B0bar@www.google.com".
  CheckAdjustedOffsets("http://foo%E3%82%B0:%E3%82%B0bar@www.google.com/",
                       kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
                       dont_omit_auth_offsets);

  const size_t view_source_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, kNpos,
    kNpos, kNpos, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
  };
  CheckAdjustedOffsets("view-source:http://foo@www.google.com/",
                       kFormatUrlOmitUsernamePassword,
                       base::UnescapeRule::NORMAL, view_source_offsets);

  const size_t idn_hostname_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 12,
    13, 14, 15, 16, 17, 18, 19
  };
  // Convert punycode to "http://\x671d\x65e5\x3042\x3055\x3072.jp/foo/".
  CheckAdjustedOffsets("http://xn--l8jvb1ey91xtjb.jp/foo/",
                       kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
                       idn_hostname_offsets_1);

  const size_t idn_hostname_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 14, 15, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 19, 20, 21, 22, 23, 24
  };
  // Convert punycode to
  // "http://test.\x89c6\x9891.\x5317\x4eac\x5927\x5b78.test/".
  CheckAdjustedOffsets("http://test.xn--cy2a840a.xn--1lq90ic7f1rc.test/",
                       kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
                       idn_hostname_offsets_2);

  const size_t unescape_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, kNpos, kNpos, 26, 27, 28, 29, 30, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, 31, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, 32, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 33, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos
  };
  // Unescape to "http://www.google.com/foo bar/\x30B0\x30FC\x30B0\x30EB".
  CheckAdjustedOffsets(
      "http://www.google.com/foo%20bar/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
      kFormatUrlOmitNothing, base::UnescapeRule::SPACES, unescape_offsets);

  const size_t ref_offsets[] = {
      0,  1,     2,     3,     4,     5,     6,     7,     8,     9,
      10, 11,    12,    13,    14,    15,    16,    17,    18,    19,
      20, 21,    22,    23,    24,    25,    26,    27,    28,    29,
      30, 31,    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      32, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 33};

  // Unescape to "http://www.google.com/foo.html#\x30B0\x30B0z".
  CheckAdjustedOffsets("http://www.google.com/foo.html#%E3%82%B0%E3%82%B0z",
                       kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
                       ref_offsets);

  const size_t omit_http_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14
  };
  CheckAdjustedOffsets("http://www.google.com/", kFormatUrlOmitHTTP,
                       base::UnescapeRule::NORMAL, omit_http_offsets);

  const size_t omit_http_start_with_ftp_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://ftp.google.com/", kFormatUrlOmitHTTP,
                       base::UnescapeRule::NORMAL,
                       omit_http_start_with_ftp_offsets);

  const size_t omit_all_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, kNpos, kNpos, kNpos, kNpos,
    0, 1, 2, 3, 4, 5, 6, 7
  };
  CheckAdjustedOffsets("http://user@foo.com/", kFormatUrlOmitDefaults,
                       base::UnescapeRule::NORMAL, omit_all_offsets);

  const size_t trim_after_host_offsets[] = {
      0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0,     1,     2,     3, 4,
      5, 6,     7,     kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 9};
  CheckAdjustedOffsets("http://foo.com/abcdefg",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);
  CheckAdjustedOffsets("http://foo.com/abc/def",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);
  CheckAdjustedOffsets("http://foo.com/abc?a=b",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);
  CheckAdjustedOffsets("http://foo.com/abc#def",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);
  CheckAdjustedOffsets("http://foo.com/a?a=b#f",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);
  CheckAdjustedOffsets("http://foo.com//??###",
                       kFormatUrlOmitDefaults | kFormatUrlTrimAfterHost,
                       base::UnescapeRule::NORMAL, trim_after_host_offsets);

  const size_t omit_https_offsets[] = {
      0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0,  1,  2, 3,
      4, 5,     6,     7,     8,     9,     10,    11,    12, 13, 14};
  CheckAdjustedOffsets("https://www.google.com/", kFormatUrlOmitHTTPS,
                       base::UnescapeRule::NORMAL, omit_https_offsets);

  const size_t omit_https_with_auth_offsets[] = {
      0,     kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0,
      kNpos, kNpos, kNpos, 0,     1,     2,     3,     4,     5,
      6,     7,     8,     9,     10,    11,    12,    13,    14};
  CheckAdjustedOffsets("https://u:p@www.google.com/",
                       kFormatUrlOmitDefaults | kFormatUrlOmitHTTPS,
                       base::UnescapeRule::NORMAL,
                       omit_https_with_auth_offsets);

  const size_t strip_trivial_subdomains_offsets_1[] = {
      0, 1,  2,  3,  4,  5,  6,  7,  kNpos, kNpos, kNpos, 7,  8,
      9, 10, 11, 12, 13, 14, 15, 16, 17,    18,    19,    20, 21};
  CheckAdjustedOffsets(
      "http://www.google.com/foo/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, strip_trivial_subdomains_offsets_1);

  const size_t strip_trivial_subdomains_from_idn_offsets[] = {
      0,     1,     2,     3,     4,     5,     6,     7,     kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 12,
      13,    14,    15,    16,    17,    18,    19};
  CheckAdjustedOffsets(
      "http://www.xn--l8jvb1ey91xtjb.jp/foo/", kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, strip_trivial_subdomains_from_idn_offsets);
}

}  // namespace url_formatter
