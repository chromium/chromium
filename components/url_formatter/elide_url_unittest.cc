// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/elide_url.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

struct Testcase {
  const std::string input;
  const std::string output;
};

struct ProgressiveTestcase {
  const std::string input;
  const std::vector<std::string> output;
};

struct UrlComponent {
  url::Parsed::ComponentType type;
  int begin;
  int len;
};

struct ParsingTestcase {
  const std::string input;
  const std::string output;
  const std::vector<UrlComponent> components;
};

#if !defined(OS_ANDROID)

// Returns the width of a utf8 or utf16 string using default UI font, or the
// provided |font_list|.
float GetWidth(const std::string& utf8,
               const gfx::FontList& font_list = gfx::FontList()) {
  return gfx::GetStringWidthF(base::UTF8ToUTF16(utf8), font_list);
}
float GetWidth(const base::string16& utf16,
               const gfx::FontList& font_list = gfx::FontList()) {
  return gfx::GetStringWidthF(utf16, font_list);
}

// Verify that one or more URLs passes through an explicit sequence of elided
// strings as available space progressively decreases. This helps ensure that
// transitional corner cases are handled properly. To be tolerant of
// character-width variation across platforms, the test allows a limited number
// of expected strings to be skipped mid-run.  The first and last expected
// strings must be matched. If the algorithm produces a string that isn't in the
// expected string list, the test fill fail. Example test expectations:
//
// google.com/intl/en/.../ads/   <-- Must match.
// google.com/intl/.../ads/
// google.com/.../ads/
// google.com/intl...   <- Elider can skip this, in case the 'l' does not fit.
// google.com/int...
// google.com/in...   <- Must match.
//
void RunProgressiveElisionTest(
    const std::vector<ProgressiveTestcase>& testcases) {
  const gfx::FontList font_list;
  for (const auto& testcase : testcases) {
    SCOPED_TRACE("Eliding " + testcase.input);
    const GURL url(testcase.input);

    // Occasionally, a parsed URL can grow in length before elision, such as
    // when parsing a Windows file path with missing slashes.
    ASSERT_FALSE(testcase.output.empty());
    float width = std::max(GetWidth(testcase.input, font_list),
                           GetWidth(testcase.output.front(), font_list));

    // Ideally, this test would iterate through all available field widths on a
    // per-pixel basis, but this is slow. Instead, compute the next input field
    // width as slightly less than the previous elided string. This approach
    // misses coverage in cases where a smaller available width generates a
    // longer string than some other larger available width, but the tradeoff
    // feels acceptable.
    int mismatches = 0;
    const int kMaxConsecutiveMismatches = 3;
    for (size_t i = 0; i < testcase.output.size(); i++) {
      const auto& expected = testcase.output[i];
      base::string16 expected_utf16 = base::UTF8ToUTF16(expected);
      base::string16 elided = url_formatter::ElideUrl(url, font_list, width);
      if (expected_utf16 != elided) {
        if (i > 0 && i < testcase.output.size() - 1 &&
            mismatches < kMaxConsecutiveMismatches) {
          mismatches++;
          continue;
        }
        EXPECT_EQ(expected_utf16, elided);
        break;
      }
      mismatches = 0;
      float new_width = GetWidth(elided, font_list);
      // Elision rounds fractional available widths up.
      EXPECT_LE(new_width, std::ceil(width)) << " at " << elided;
      width = new_width - 1.0f;
    }
  }
}

void RunElisionTest(const std::vector<Testcase>& testcases) {
  const gfx::FontList font_list;
  for (const auto& testcase : testcases) {
    SCOPED_TRACE("Eliding " + testcase.input);
    const GURL url(testcase.input);
    const float available_width = GetWidth(testcase.output, font_list);
    EXPECT_EQ(base::UTF8ToUTF16(testcase.output),
              url_formatter::ElideUrl(url, font_list, available_width));
  }
}

// Test eliding of commonplace URLs.
TEST(TextEliderTest, TestGeneralEliding) {
  const std::string kEllipsisStr(gfx::kEllipsis);
  const std::vector<ProgressiveTestcase> progressive_testcases = {
      // Elide a non-www URL (www URLs are handled differently). In this first
      // case, elide down to nothing to test the terminal cases.
      {"http://xyz.google.com/foo?bar",
       {
           /* clang-format off */
           "xyz.google.com/foo?bar",
           "xyz.google.com/foo?b" + kEllipsisStr,
           "xyz.google.com/foo?" + kEllipsisStr,
           "xyz.google.com/foo" + kEllipsisStr,
           "xyz.google.com/fo" + kEllipsisStr,
           "xyz.google.com/f" + kEllipsisStr,
           kEllipsisStr + "google.com/foo" + kEllipsisStr,
           kEllipsisStr + "google.com/fo" + kEllipsisStr,
           kEllipsisStr + "google.com/f" + kEllipsisStr,
           kEllipsisStr + "google.com/" + kEllipsisStr,
           kEllipsisStr + "google.com" + kEllipsisStr,
           kEllipsisStr + "google.co" + kEllipsisStr,
           kEllipsisStr + "google.c" + kEllipsisStr,
           kEllipsisStr + "google." + kEllipsisStr,
           kEllipsisStr + "google" + kEllipsisStr,
           kEllipsisStr + "googl" + kEllipsisStr,
           kEllipsisStr + "goog" + kEllipsisStr,
           kEllipsisStr + "goo" + kEllipsisStr,
           kEllipsisStr + "go" + kEllipsisStr,
           kEllipsisStr + "g" + kEllipsisStr,
           kEllipsisStr + kEllipsisStr,
           kEllipsisStr,
           ""
           /* clang-format on */
       }},
      // The trailing directory name is preserved
      {"http://www.google.com/intl/en/ads/",
       {
           /* clang-format off */
           "www.google.com/intl/en/ads/",
           "google.com/intl/en/ads/",
           "google.com/intl/" + kEllipsisStr + "/ads/",
           "google.com/" + kEllipsisStr + "/ads/",
           "google.com/" + kEllipsisStr + "/ad" + kEllipsisStr,
           "google.com/" + kEllipsisStr + "/a" + kEllipsisStr,
           "google.com/intl/e" + kEllipsisStr,
           "google.com/intl/" + kEllipsisStr,
           "google.com/intl" + kEllipsisStr,
           "google.com/int" + kEllipsisStr,
           "google.com/in" + kEllipsisStr,
           "google.com/i" + kEllipsisStr,
           "google.com/" + kEllipsisStr,
           "google.com" + kEllipsisStr,
           "google.co" + kEllipsisStr,
           "google.c" + kEllipsisStr,
           "google." + kEllipsisStr,
           "google" + kEllipsisStr,
           "googl" + kEllipsisStr,
           /* clang-format on */
       }},
      // Subdomain is completely elided if the last path element doesn't fit.
      {"https://subdomain.foo.com/bar/filename.html",
       {
           /* clang-format off */
           "https://subdomain.foo.com/bar/filename.html",
           "subdomain.foo.com/bar/filename.html",
           "subdomain.foo.com/" + kEllipsisStr + "/filename.html",
           kEllipsisStr + "foo.com/bar/filename.html",
           kEllipsisStr + "foo.com/" + kEllipsisStr + "/filename.html",
           /* clang-format on */
       }},
      // Path eliding works when a query is present.
      {"http://www.g.com/subdir/ads/?query",
       {
           /* clang-format off */
           "www.g.com/subdir/ads/?query",
           "www.g.com/subdir/ads/?que" + kEllipsisStr,
           "www.g.com/subdir/ads/?qu" + kEllipsisStr,
           "www.g.com/subdir/ads/?q" + kEllipsisStr,
           "www.g.com/subdir/ads/?" + kEllipsisStr,
           "www.g.com/subdir/ads/" + kEllipsisStr,
           "www.g.com/subdir/ads" + kEllipsisStr,
           "www.g.com/subdir/ad" + kEllipsisStr,
           /* clang-format on */
       }},
  };
  RunProgressiveElisionTest(progressive_testcases);
}

// When there is very little space available, the elision code will shorten
// both path AND file name to an ellipsis - ".../...". To avoid this result,
// there is a hack in place that simply treats them as one string in this
// case.
TEST(TextEliderTest, TestTrailingEllipsisSlashEllipsisHack) {
  const std::string kEllipsisStr(gfx::kEllipsis);

  // Very little space, would cause double ellipsis.
  gfx::FontList font_list;
  GURL url("http://battersbox.com/directory/foo/peter_paul_and_mary.html");
  float available_width = GetWidth(
      "battersbox.com/" + kEllipsisStr + "/" + kEllipsisStr, font_list);

  // Create the expected string, after elision. Depending on font size, the
  // directory might become /dir... or /di... or/d... - it never should be
  // shorter than that. (If it is, the font considers d... to be longer
  // than .../... -  that should never happen).
  ASSERT_GT(GetWidth(kEllipsisStr + "/" + kEllipsisStr, font_list),
            GetWidth("d" + kEllipsisStr, font_list));
  GURL long_url("http://battersbox.com/directorynameisreallylongtoforcetrunc");
  base::string16 expected =
      url_formatter::ElideUrl(long_url, font_list, available_width);
  // Ensure that the expected result still contains part of the directory name.
  ASSERT_GT(expected.length(), std::string("battersbox.com/d").length());
  EXPECT_EQ(expected, url_formatter::ElideUrl(url, font_list, available_width));

  // Regression test for https://crbug.com/756717. An empty path, eliding to a
  // width in between the full domain ("www.angelfire.lycos.com") and a bit
  // longer than the ETLD+1 ("…lycos.com…/…UV"). This previously crashed due to
  // the path being empty.
  url = GURL("http://www.angelfire.lycos.com/");
  available_width = GetWidth(kEllipsisStr + "angelfire.lycos.com", font_list);
  EXPECT_EQ(base::UTF8ToUTF16(kEllipsisStr + "lycos.com"),
            url_formatter::ElideUrl(url, font_list, available_width));

  // More space available - elide directories, partially elide filename.
  const std::vector<Testcase> testcases = {
      {"http://battersbox.com/directory/foo/peter_paul_and_mary.html",
       "battersbox.com/" + kEllipsisStr + "/peter" + kEllipsisStr},
  };
  RunElisionTest(testcases);
}

// Test eliding of empty strings, URLs with ports, passwords, queries, etc.
TEST(TextEliderTest, TestElisionSpecialCases) {
#if defined(OS_WIN)
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
#endif
  const std::string kEllipsisStr(gfx::kEllipsis);
  const std::vector<Testcase> testcases = {
      // URL with "www" subdomain (gets removed specially).
      {"http://www.google.com/foo?bar", "www.google.com/foo?bar"},
      {"http://www.google.com/foo?bar", "google.com/foo?bar"},

      // URL with no path.
      {"http://xyz.google.com", kEllipsisStr + "google.com"},
      {"https://xyz.google.com", kEllipsisStr + "google.com"},

      {"http://a.b.com/pathname/c?d", "a.b.com/" + kEllipsisStr + "/c?d"},
      {"", ""},
      {"http://foo.bar..example.com...hello/test/filename.html",
       "foo.bar..example.com...hello/" + kEllipsisStr + "/filename.html"},
      {"http://foo.bar../", "foo.bar.."},
      {"http://xn--1lq90i.cn/foo", "\xe5\x8c\x97\xe4\xba\xac.cn/foo"},
      {"http://me:mypass@secrethost.com:99/foo?bar#baz",
       "secrethost.com:99/foo?bar#baz"},
      {"http://me:mypass@ss%xxfdsf.com/foo", "ss%25xxfdsf.com/foo"},
      {"mailto:elgoato@elgoato.com", "mailto:elgoato@elgoato.com"},
      {"javascript:click(0)", "javascript:click(0)"},
      {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
       "chess.eecs.berkeley.edu:4430/login/arbitfilename"},
      {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
       kEllipsisStr + "berkeley.edu:4430/" + kEllipsisStr + "/arbitfilename"},

      // Unescaping.
      {"http://www/%E4%BD%A0%E5%A5%BD?"
       "q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0\xe4\xbd\xa0\xe4\xbd\xa0",
       "www/\xe4\xbd\xa0\xe5\xa5\xbd?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0" +
           kEllipsisStr},

      // Invalid unescaping for path. The ref will always be valid UTF-8. We
      // don't bother to do too many edge cases, since these are handled by the
      // escaper unittest.
      {"http://www/%E4%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
       "www/%E4%A0%E5%A5%BD?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0"},
  };

  RunElisionTest(testcases);
}

// Test eliding of file: URLs.
TEST(TextEliderTest, TestFileURLEliding) {
  const std::string kEllipsisStr(gfx::kEllipsis);
  const std::vector<ProgressiveTestcase> progressive_testcases = {
    {"file:///C:/path1/path2/path3/filename",
     {
         /* clang-format off */
         "file:///C:/path1/path2/path3/filename",
         "/C:/path1/path2/path3/filename",
         "C:/path1/path2/path3/filename",
         "C:/path1/path2/" + kEllipsisStr + "/filename",
         /* clang-format on */
     }},
// GURL parses "file:///C:path" differently on windows than it does on posix.
#if defined(OS_WIN)
    {"file:///C:path1/path2/path3/filename",
     {
         /* clang-format off */
         "C:/path1/path2/path3/filename",
         "C:/path1/path2/" + kEllipsisStr + "/filename",
         "C:/path1/" + kEllipsisStr + "/filename",
         "C:/" + kEllipsisStr + "/filename",
         /* clang-format on */
     }},
#endif  // defined(OS_WIN)
    {"file://filer/foo/bar/file",
     {
         /* clang-format off */
         "file://filer/foo/bar/file",
         "filer/foo/bar/file",
         "filer/foo/" + kEllipsisStr + "/file",
         "filer/" + kEllipsisStr + "/file",
         "filer/foo" + kEllipsisStr,
         "filer/fo" + kEllipsisStr,
         "filer/f" + kEllipsisStr,
         "filer/" + kEllipsisStr,
         "filer" + kEllipsisStr,
         "file" + kEllipsisStr,
         /* clang-format on */
     }},
  };

  RunProgressiveElisionTest(progressive_testcases);

  const std::vector<Testcase> testcases = {
      // Eliding file URLs with nothing after the ':' shouldn't crash.
      {"file:///aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:", "aaa" + kEllipsisStr},
      {"file:///aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:/", "aaa" + kEllipsisStr},
  };
  RunElisionTest(testcases);
}

TEST(TextEliderTest, TestHostEliding) {
  const std::string kEllipsisStr(gfx::kEllipsis);
  Testcase testcases[] = {
    {"http://google.com", "google.com"},
    {"http://reallyreallyreallylongdomainname.com",
     "reallyreallyreallylongdomainname.com"},
    {"http://foo", "foo"},
    {"http://foo.bar", "foo.bar"},
    {"http://subdomain.google.com", kEllipsisStr + ".google.com"},
    {"http://a.b.c.d.e.f.com", kEllipsisStr + "f.com"},
    {"http://subdomain.foo.bar", kEllipsisStr + "in.foo.bar"},
    {"http://subdomain.reallylongdomainname.com",
     kEllipsisStr + "ain.reallylongdomainname.com"},
    {"http://a.b.c.d.e.f.com", kEllipsisStr + ".e.f.com"},
    // IDN - Greek alpha.beta.gamma.delta.epsilon.zeta.com
    {"http://xn--mxa.xn--nxa.xn--oxa.xn--pxa.xn--qxa.xn--rxa.com",
     kEllipsisStr + ".\xCE\xB5.\xCE\xB6.com"},
  };

  for (size_t i = 0; i < base::size(testcases); ++i) {
    const float available_width = GetWidth(testcases[i].output);
    EXPECT_EQ(base::UTF8ToUTF16(testcases[i].output),
              url_formatter::ElideHost(GURL(testcases[i].input),
                                       gfx::FontList(), available_width));
  }

  // Trying to elide to a really short length will still keep the full TLD+1
  EXPECT_EQ(
      base::ASCIIToUTF16("google.com"),
      url_formatter::ElideHost(GURL("http://google.com"), gfx::FontList(), 2));
  EXPECT_EQ(base::UTF8ToUTF16(kEllipsisStr + ".google.com"),
            url_formatter::ElideHost(GURL("http://subdomain.google.com"),
                                     gfx::FontList(), 2));
  EXPECT_EQ(
      base::ASCIIToUTF16("foo.bar"),
      url_formatter::ElideHost(GURL("http://foo.bar"), gfx::FontList(), 2));
}

#endif  // !defined(OS_ANDROID)

struct OriginTestData {
  const char* const description;
  const char* const input;
  const wchar_t* const output;
  const wchar_t* const output_omit_web_scheme;
  const wchar_t* const output_omit_cryptographic_scheme;
};

// Common test data for both FormatUrlForSecurityDisplay() and
// FormatOriginForSecurityDisplay()
const OriginTestData common_tests[] = {
    {"Empty URL", "", L"", L"", L""},
    {"HTTP URL", "http://www.google.com/", L"http://www.google.com",
     L"www.google.com", L"http://www.google.com"},
    {"HTTPS URL", "https://www.google.com/", L"https://www.google.com",
     L"www.google.com", L"www.google.com"},
    {"Standard HTTP port", "http://www.google.com:80/",
     L"http://www.google.com", L"www.google.com", L"http://www.google.com"},
    {"Standard HTTPS port", "https://www.google.com:443/",
     L"https://www.google.com", L"www.google.com", L"www.google.com"},
    {"Standard HTTP port, IDN Chinese",
     "http://\xe4\xb8\xad\xe5\x9b\xbd.icom.museum:80",
     L"http://\x4e2d\x56fd.icom.museum", L"\x4e2d\x56fd.icom.museum",
     L"http://\x4e2d\x56fd.icom.museum"},
    {"HTTP URL, IDN Hebrew (RTL)",
     "http://"
     "\xd7\x90\xd7\x99\xd7\xa7\xd7\x95\xd7\xb4\xd7\x9d."
     "\xd7\x99\xd7\xa9\xd7\xa8\xd7\x90\xd7\x9c.museum/",
     L"http://xn--4dbklr2c8d.xn--4dbrk0ce.museum",
     L"xn--4dbklr2c8d.xn--4dbrk0ce.museum",
     L"http://xn--4dbklr2c8d.xn--4dbrk0ce.museum"},
    {"HTTP URL with query string, IDN Arabic (RTL)",
     "http://\xd9\x85\xd8\xb5\xd8\xb1.icom.museum/foo.html?yes=no",
     L"http://xn--wgbh1c.icom.museum", L"xn--wgbh1c.icom.museum",
     L"http://xn--wgbh1c.icom.museum"},
    {"Non-standard HTTP port", "http://www.google.com:9000/",
     L"http://www.google.com:9000", L"www.google.com:9000",
     L"http://www.google.com:9000"},
    {"Non-standard HTTPS port", "https://www.google.com:9000/",
     L"https://www.google.com:9000", L"www.google.com:9000",
     L"www.google.com:9000"},
    {"HTTP URL with path", "http://www.google.com/test.html",
     L"http://www.google.com", L"www.google.com", L"http://www.google.com"},
    {"HTTPS URL with path", "https://www.google.com/test.html",
     L"https://www.google.com", L"www.google.com", L"www.google.com"},
    {"Unusual secure scheme (wss)", "wss://www.google.com/",
     L"wss://www.google.com", L"wss://www.google.com", L"www.google.com"},
    {"Unusual non-secure scheme (ftp)", "ftp://www.google.com/",
     L"ftp://www.google.com", L"ftp://www.google.com", L"ftp://www.google.com"},
    {"Unlisted scheme (chrome)", "chrome://version", L"chrome://version",
     L"chrome://version", L"chrome://version"},
    {"HTTP IP address", "http://173.194.65.103", L"http://173.194.65.103",
     L"173.194.65.103", L"http://173.194.65.103"},
    {"HTTPS IP address", "https://173.194.65.103", L"https://173.194.65.103",
     L"173.194.65.103", L"173.194.65.103"},
    {"HTTP IPv6 address", "http://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]/",
     L"http://[fe80::202:b3ff:fe1e:8329]", L"[fe80::202:b3ff:fe1e:8329]",
     L"http://[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPs IPv6 address", "https://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]/",
     L"https://[fe80::202:b3ff:fe1e:8329]", L"[fe80::202:b3ff:fe1e:8329]",
     L"[fe80::202:b3ff:fe1e:8329]"},
    {"HTTP IPv6 address with port",
     "http://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:80/",
     L"http://[fe80::202:b3ff:fe1e:8329]", L"[fe80::202:b3ff:fe1e:8329]",
     L"http://[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPs IPv6 address with port",
     "https://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:443/",
     L"https://[fe80::202:b3ff:fe1e:8329]", L"[fe80::202:b3ff:fe1e:8329]",
     L"[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPS IP address, non-default port", "https://173.194.65.103:8443",
     L"https://173.194.65.103:8443", L"173.194.65.103:8443",
     L"173.194.65.103:8443"},
    {"Invalid host 1", "https://www.cyber../wow.php", L"https://www.cyber..",
     L"www.cyber..", L"www.cyber.."},
    {"Invalid host 2", "https://www...cyber/wow.php", L"https://www...cyber",
     L"www...cyber", L"www...cyber"},
    {"Invalid port 3", "https://173.194.65.103:/hello.aspx",
     L"https://173.194.65.103", L"173.194.65.103", L"173.194.65.103"},
    {"Trailing dot in DNS name", "https://www.example.com./get/goat",
     L"https://www.example.com.", L"www.example.com.", L"www.example.com."}};

TEST(TextEliderTest, FormatUrlForSecurityDisplay) {
  for (size_t i = 0; i < base::size(common_tests); ++i) {
    base::string16 formatted =
        url_formatter::FormatUrlForSecurityDisplay(GURL(common_tests[i].input));
    EXPECT_EQ(base::WideToUTF16(common_tests[i].output), formatted)
        << common_tests[i].description;

    base::string16 formatted_omit_web_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(common_tests[i].input),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(base::WideToUTF16(common_tests[i].output_omit_web_scheme),
              formatted_omit_web_scheme)
        << common_tests[i].description;

    base::string16 formatted_omit_cryptographic_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(common_tests[i].input),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(
        base::WideToUTF16(common_tests[i].output_omit_cryptographic_scheme),
        formatted_omit_cryptographic_scheme)
        << common_tests[i].description;
  }

  const OriginTestData tests[] = {
      {"File URI", "file:///usr/example/file.html",
       L"file:///usr/example/file.html", L"file:///usr/example/file.html",
       L"file:///usr/example/file.html"},
      {"File URI with hostname", "file://localhost/usr/example/file.html",
       L"file:///usr/example/file.html", L"file:///usr/example/file.html",
       L"file:///usr/example/file.html"},
      {"UNC File URI 1", "file:///CONTOSO/accounting/money.xls",
       L"file:///CONTOSO/accounting/money.xls",
       L"file:///CONTOSO/accounting/money.xls",
       L"file:///CONTOSO/accounting/money.xls"},
      {"UNC File URI 2",
       "file:///C:/Program%20Files/Music/Web%20Sys/main.html?REQUEST=RADIO",
       L"file:///C:/Program%20Files/Music/Web%20Sys/main.html",
       L"file:///C:/Program%20Files/Music/Web%20Sys/main.html",
       L"file:///C:/Program%20Files/Music/Web%20Sys/main.html"},
      {"Invalid IPv6 address", "https://[2001:db8:0:1]/",
       L"https://[2001:db8:0:1]", L"https://[2001:db8:0:1]",
       L"https://[2001:db8:0:1]"},
      {"HTTP filesystem: URL with path",
       "filesystem:http://www.google.com/temporary/test.html",
       L"filesystem:http://www.google.com", L"filesystem:http://www.google.com",
       L"filesystem:http://www.google.com"},
      {"File filesystem: URL with path",
       "filesystem:file://localhost/temporary/stuff/"
       "test.html?z=fun&goat=billy",
       L"filesystem:file:///temporary/stuff/test.html",
       L"filesystem:file:///temporary/stuff/test.html",
       L"filesystem:file:///temporary/stuff/test.html"},
      {"Invalid scheme 1", "twelve://www.cyber.org/wow.php",
       L"twelve://www.cyber.org/wow.php", L"twelve://www.cyber.org/wow.php",
       L"twelve://www.cyber.org/wow.php"},
      {"Invalid scheme 2", "://www.cyber.org/wow.php",
       L"://www.cyber.org/wow.php", L"://www.cyber.org/wow.php",
       L"://www.cyber.org/wow.php"},
      {"Invalid port 1", "https://173.194.65.103:000",
       L"https://173.194.65.103:0", L"173.194.65.103:0", L"173.194.65.103:0"},
      {"Invalid port 2", "https://173.194.65.103:gruffle",
       L"https://173.194.65.103:gruffle", L"https://173.194.65.103:gruffle",
       L"https://173.194.65.103:gruffle"},
      {"Blob URL",
       "blob:http://www.html5rocks.com/4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       L"blob:http://www.html5rocks.com/"
       L"4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       L"blob:http://www.html5rocks.com/"
       L"4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       L"blob:http://www.html5rocks.com/"
       L"4d4ff040-6d61-4446-86d3-13ca07ec9ab9"}};

  for (size_t i = 0; i < base::size(tests); ++i) {
    base::string16 formatted =
        url_formatter::FormatUrlForSecurityDisplay(GURL(tests[i].input));
    EXPECT_EQ(base::WideToUTF16(tests[i].output), formatted)
        << tests[i].description;

    base::string16 formatted_omit_web_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(tests[i].input),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(base::WideToUTF16(tests[i].output_omit_web_scheme),
              formatted_omit_web_scheme)
        << tests[i].description;

    base::string16 formatted_omit_cryptographic_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(tests[i].input),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(base::WideToUTF16(tests[i].output_omit_cryptographic_scheme),
              formatted_omit_cryptographic_scheme)
        << tests[i].description;
  }

  base::string16 formatted = url_formatter::FormatUrlForSecurityDisplay(GURL());
  EXPECT_EQ(base::string16(), formatted)
      << "Explicitly test the 0-argument GURL constructor";

  base::string16 formatted_omit_scheme =
      url_formatter::FormatUrlForSecurityDisplay(
          GURL(), url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  EXPECT_EQ(base::string16(), formatted_omit_scheme)
      << "Explicitly test the 0-argument GURL constructor";

  formatted_omit_scheme = url_formatter::FormatUrlForSecurityDisplay(
      GURL(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  EXPECT_EQ(base::string16(), formatted_omit_scheme)
      << "Explicitly test the 0-argument GURL constructor";
}

TEST(TextEliderTest, FormatOriginForSecurityDisplay) {
  for (size_t i = 0; i < base::size(common_tests); ++i) {
    base::string16 formatted = url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(GURL(common_tests[i].input)));
    EXPECT_EQ(base::WideToUTF16(common_tests[i].output), formatted)
        << common_tests[i].description;

    base::string16 formatted_omit_web_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(common_tests[i].input)),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(base::WideToUTF16(common_tests[i].output_omit_web_scheme),
              formatted_omit_web_scheme)
        << common_tests[i].description;

    base::string16 formatted_omit_cryptographic_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(common_tests[i].input)),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(
        base::WideToUTF16(common_tests[i].output_omit_cryptographic_scheme),
        formatted_omit_cryptographic_scheme)
        << common_tests[i].description;
  }

  const OriginTestData tests[] = {
      {"File URI", "file:///usr/example/file.html", L"file://", L"file://",
       L"file://"},
      {"File URI with hostname", "file://localhost/usr/example/file.html",
       L"file://localhost", L"file://localhost", L"file://localhost"},
      {"UNC File URI 1", "file:///CONTOSO/accounting/money.xls", L"file://",
       L"file://", L"file://"},
      {"UNC File URI 2",
       "file:///C:/Program%20Files/Music/Web%20Sys/main.html?REQUEST=RADIO",
       L"file://", L"file://", L"file://"},
      {"Invalid IPv6 address", "https://[2001:db8:0:1]/", L"", L"", L""},
      {"HTTP filesystem: URL with path",
       "filesystem:http://www.google.com/temporary/test.html",
       L"http://www.google.com", L"www.google.com", L"http://www.google.com"},
      {"File filesystem: URL with path",
       "filesystem:file://localhost/temporary/stuff/test.html?z=fun&goat=billy",
       L"file://", L"file://", L"file://"},
      {"Invalid scheme 1", "twelve://www.cyber.org/wow.php", L"", L"", L""},
      {"Invalid scheme 2", "://www.cyber.org/wow.php", L"", L"", L""},
      {"Invalid port 1", "https://173.194.65.103:99999", L"", L"", L""},
      {"Invalid port 2", "https://173.194.65.103:gruffle", L"", L"", L""},
      {"Blob URL",
       "blob:http://www.html5rocks.com/4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       L"http://www.html5rocks.com", L"www.html5rocks.com",
       L"http://www.html5rocks.com"}};

  for (size_t i = 0; i < base::size(tests); ++i) {
    base::string16 formatted = url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(GURL(tests[i].input)));
    EXPECT_EQ(base::WideToUTF16(tests[i].output), formatted)
        << tests[i].description;

    base::string16 formatted_omit_web_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(tests[i].input)),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(base::WideToUTF16(tests[i].output_omit_web_scheme),
              formatted_omit_web_scheme)
        << tests[i].description;

    base::string16 formatted_omit_cryptographic_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(tests[i].input)),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(base::WideToUTF16(tests[i].output_omit_cryptographic_scheme),
              formatted_omit_cryptographic_scheme)
        << tests[i].description;
  }

  base::string16 formatted = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(GURL()));
  EXPECT_EQ(base::string16(), formatted)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";

  base::string16 formatted_omit_scheme =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL()),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  EXPECT_EQ(base::string16(), formatted_omit_scheme)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";

  formatted_omit_scheme = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(GURL()),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  EXPECT_EQ(base::string16(), formatted_omit_scheme)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";
}

}  // namespace
