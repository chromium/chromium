// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/elide_url.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
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

#if !BUILDFLAG(IS_ANDROID)

// Returns the width of a utf8 or utf16 string using default UI font, or the
// provided |font_list|.
float GetWidth(const std::string& utf8,
               const gfx::FontList& font_list = gfx::FontList()) {
  return gfx::GetStringWidthF(base::UTF8ToUTF16(utf8), font_list);
}
float GetWidth(const std::u16string& utf16,
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
      std::u16string expected_utf16 = base::UTF8ToUTF16(expected);
      std::u16string elided = url_formatter::ElideUrl(url, font_list, width);
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
  std::u16string expected =
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
#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_WIN)
    {"file:///C:path1/path2/path3/filename",
     {
         /* clang-format off */
         "C:/path1/path2/path3/filename",
         "C:/path1/path2/" + kEllipsisStr + "/filename",
         "C:/path1/" + kEllipsisStr + "/filename",
         "C:/" + kEllipsisStr + "/filename",
         /* clang-format on */
     }},
#endif  // BUILDFLAG(IS_WIN)
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

  for (const auto& testcase : testcases) {
    const float available_width = GetWidth(testcase.output);
    EXPECT_EQ(base::UTF8ToUTF16(testcase.output),
              url_formatter::ElideHost(GURL(testcase.input), gfx::FontList(),
                                       available_width));
  }

  // Trying to elide to a really short length will still keep the full TLD+1
  EXPECT_EQ(u"google.com", url_formatter::ElideHost(GURL("http://google.com"),
                                                    gfx::FontList(), 2));
  EXPECT_EQ(base::UTF8ToUTF16(kEllipsisStr + ".google.com"),
            url_formatter::ElideHost(GURL("http://subdomain.google.com"),
                                     gfx::FontList(), 2));
  EXPECT_EQ(u"foo.bar", url_formatter::ElideHost(GURL("http://foo.bar"),
                                                 gfx::FontList(), 2));
}

#endif  // !BUILDFLAG(IS_ANDROID)

struct OriginTestData {
  const char* const description;
  const char* const input;
  const char16_t* const output;
  const char16_t* const output_omit_web_scheme;
  const char16_t* const output_omit_cryptographic_scheme;
};

// Common test data for both FormatUrlForSecurityDisplay() and
// FormatOriginForSecurityDisplay()
const OriginTestData common_tests[] = {
    {"Empty URL", "", u"", u"", u""},
    {"HTTP URL", "http://www.google.com/", u"http://www.google.com",
     u"www.google.com", u"http://www.google.com"},
    {"HTTPS URL", "https://www.google.com/", u"https://www.google.com",
     u"www.google.com", u"www.google.com"},
    {"Standard HTTP port", "http://www.google.com:80/",
     u"http://www.google.com", u"www.google.com", u"http://www.google.com"},
    {"Standard HTTPS port", "https://www.google.com:443/",
     u"https://www.google.com", u"www.google.com", u"www.google.com"},
    {"Standard HTTP port, IDN Chinese",
     "http://\xe4\xb8\xad\xe5\x9b\xbd.icom.museum:80",
     u"http://中国.icom.museum", u"中国.icom.museum",
     u"http://中国.icom.museum"},
    {"HTTP URL, IDN Hebrew (RTL)",
     "http://"
     "\xd7\x90\xd7\x99\xd7\xa7\xd7\x95\xd7\xb4\xd7\x9d."
     "\xd7\x99\xd7\xa9\xd7\xa8\xd7\x90\xd7\x9c.museum/",
     u"http://xn--4dbklr2c8d.xn--4dbrk0ce.museum",
     u"xn--4dbklr2c8d.xn--4dbrk0ce.museum",
     u"http://xn--4dbklr2c8d.xn--4dbrk0ce.museum"},
    {"HTTP URL with query string, IDN Arabic (RTL)",
     "http://\xd9\x85\xd8\xb5\xd8\xb1.icom.museum/foo.html?yes=no",
     u"http://xn--wgbh1c.icom.museum", u"xn--wgbh1c.icom.museum",
     u"http://xn--wgbh1c.icom.museum"},
    {"Non-standard HTTP port", "http://www.google.com:9000/",
     u"http://www.google.com:9000", u"www.google.com:9000",
     u"http://www.google.com:9000"},
    {"Non-standard HTTPS port", "https://www.google.com:9000/",
     u"https://www.google.com:9000", u"www.google.com:9000",
     u"www.google.com:9000"},
    {"HTTP URL with path", "http://www.google.com/test.html",
     u"http://www.google.com", u"www.google.com", u"http://www.google.com"},
    {"HTTPS URL with path", "https://www.google.com/test.html",
     u"https://www.google.com", u"www.google.com", u"www.google.com"},
    {"Unusual secure scheme (wss)", "wss://www.google.com/",
     u"wss://www.google.com", u"wss://www.google.com", u"www.google.com"},
    {"Unusual non-secure scheme (ftp)", "ftp://www.google.com/",
     u"ftp://www.google.com", u"ftp://www.google.com", u"ftp://www.google.com"},
    {"Unlisted scheme (chrome)", "chrome://version", u"chrome://version",
     u"chrome://version", u"chrome://version"},
    {"HTTP IP address", "http://173.194.65.103", u"http://173.194.65.103",
     u"173.194.65.103", u"http://173.194.65.103"},
    {"HTTPS IP address", "https://173.194.65.103", u"https://173.194.65.103",
     u"173.194.65.103", u"173.194.65.103"},
    {"HTTP IPv6 address", "http://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]/",
     u"http://[fe80::202:b3ff:fe1e:8329]", u"[fe80::202:b3ff:fe1e:8329]",
     u"http://[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPs IPv6 address", "https://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]/",
     u"https://[fe80::202:b3ff:fe1e:8329]", u"[fe80::202:b3ff:fe1e:8329]",
     u"[fe80::202:b3ff:fe1e:8329]"},
    {"HTTP IPv6 address with port",
     "http://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:80/",
     u"http://[fe80::202:b3ff:fe1e:8329]", u"[fe80::202:b3ff:fe1e:8329]",
     u"http://[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPs IPv6 address with port",
     "https://[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:443/",
     u"https://[fe80::202:b3ff:fe1e:8329]", u"[fe80::202:b3ff:fe1e:8329]",
     u"[fe80::202:b3ff:fe1e:8329]"},
    {"HTTPS IP address, non-default port", "https://173.194.65.103:8443",
     u"https://173.194.65.103:8443", u"173.194.65.103:8443",
     u"173.194.65.103:8443"},
    {"Invalid host 1", "https://www.cyber../wow.php", u"https://www.cyber..",
     u"www.cyber..", u"www.cyber.."},
    {"Invalid host 2", "https://www...cyber/wow.php", u"https://www...cyber",
     u"www...cyber", u"www...cyber"},
    {"Invalid port 3", "https://173.194.65.103:/hello.aspx",
     u"https://173.194.65.103", u"173.194.65.103", u"173.194.65.103"},
    {"Trailing dot in DNS name", "https://www.example.com./get/goat",
     u"https://www.example.com.", u"www.example.com.", u"www.example.com."}};

TEST(TextEliderTest, FormatUrlForSecurityDisplay) {
  for (const auto& common_test : common_tests) {
    std::u16string formatted =
        url_formatter::FormatUrlForSecurityDisplay(GURL(common_test.input));
    EXPECT_EQ(common_test.output, formatted) << common_test.description;

    std::u16string formatted_omit_web_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(common_test.input),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(common_test.output_omit_web_scheme, formatted_omit_web_scheme)
        << common_test.description;

    std::u16string formatted_omit_cryptographic_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(common_test.input),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(common_test.output_omit_cryptographic_scheme,
              formatted_omit_cryptographic_scheme)
        << common_test.description;
  }

  const OriginTestData tests[] = {
      {"File URI", "file:///usr/example/file.html",
       u"file:///usr/example/file.html", u"file:///usr/example/file.html",
       u"file:///usr/example/file.html"},
      {"File URI with hostname", "file://localhost/usr/example/file.html",
       u"file:///usr/example/file.html", u"file:///usr/example/file.html",
       u"file:///usr/example/file.html"},
      {"UNC File URI 1", "file:///CONTOSO/accounting/money.xls",
       u"file:///CONTOSO/accounting/money.xls",
       u"file:///CONTOSO/accounting/money.xls",
       u"file:///CONTOSO/accounting/money.xls"},
      {"UNC File URI 2",
       "file:///C:/Program%20Files/Music/Web%20Sys/main.html?REQUEST=RADIO",
       u"file:///C:/Program%20Files/Music/Web%20Sys/main.html",
       u"file:///C:/Program%20Files/Music/Web%20Sys/main.html",
       u"file:///C:/Program%20Files/Music/Web%20Sys/main.html"},
      {"Invalid IPv6 address", "https://[2001:db8:0:1]/",
       u"https://[2001:db8:0:1]", u"https://[2001:db8:0:1]",
       u"https://[2001:db8:0:1]"},
      {"HTTP filesystem: URL with path",
       "filesystem:http://www.google.com/temporary/test.html",
       u"filesystem:http://www.google.com", u"filesystem:http://www.google.com",
       u"filesystem:http://www.google.com"},
      {"File filesystem: URL with path",
       "filesystem:file://localhost/temporary/stuff/"
       "test.html?z=fun&goat=billy",
       u"filesystem:file:///temporary/stuff/test.html",
       u"filesystem:file:///temporary/stuff/test.html",
       u"filesystem:file:///temporary/stuff/test.html"},
      {"Invalid scheme 1", "twelve://www.cyber.org/wow.php",
       u"twelve://www.cyber.org/wow.php", u"twelve://www.cyber.org/wow.php",
       u"twelve://www.cyber.org/wow.php"},
      {"Invalid scheme 2", "://www.cyber.org/wow.php",
       u"://www.cyber.org/wow.php", u"://www.cyber.org/wow.php",
       u"://www.cyber.org/wow.php"},
      {"Invalid port 1", "https://173.194.65.103:000",
       u"https://173.194.65.103:0", u"173.194.65.103:0", u"173.194.65.103:0"},
      {"Invalid port 2", "https://173.194.65.103:gruffle",
       u"https://173.194.65.103:gruffle", u"https://173.194.65.103:gruffle",
       u"https://173.194.65.103:gruffle"},
      {"Blob URL",
       "blob:http://www.html5rocks.com/4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       u"blob:http://www.html5rocks.com/"
       u"4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       u"blob:http://www.html5rocks.com/"
       u"4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       u"blob:http://www.html5rocks.com/"
       u"4d4ff040-6d61-4446-86d3-13ca07ec9ab9"}};

  for (const auto& test : tests) {
    std::u16string formatted =
        url_formatter::FormatUrlForSecurityDisplay(GURL(test.input));
    EXPECT_EQ(test.output, formatted) << test.description;

    std::u16string formatted_omit_web_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(test.input),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(test.output_omit_web_scheme, formatted_omit_web_scheme)
        << test.description;

    std::u16string formatted_omit_cryptographic_scheme =
        url_formatter::FormatUrlForSecurityDisplay(
            GURL(test.input), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(test.output_omit_cryptographic_scheme,
              formatted_omit_cryptographic_scheme)
        << test.description;
  }

  std::u16string formatted = url_formatter::FormatUrlForSecurityDisplay(GURL());
  EXPECT_EQ(std::u16string(), formatted)
      << "Explicitly test the 0-argument GURL constructor";

  std::u16string formatted_omit_scheme =
      url_formatter::FormatUrlForSecurityDisplay(
          GURL(), url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  EXPECT_EQ(std::u16string(), formatted_omit_scheme)
      << "Explicitly test the 0-argument GURL constructor";

  formatted_omit_scheme = url_formatter::FormatUrlForSecurityDisplay(
      GURL(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  EXPECT_EQ(std::u16string(), formatted_omit_scheme)
      << "Explicitly test the 0-argument GURL constructor";
}

TEST(TextEliderTest, FormatOriginForSecurityDisplay) {
  for (const auto& common_test : common_tests) {
    std::u16string formatted = url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(GURL(common_test.input)));
    EXPECT_EQ(common_test.output, formatted) << common_test.description;

    std::u16string formatted_omit_web_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(common_test.input)),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(common_test.output_omit_web_scheme, formatted_omit_web_scheme)
        << common_test.description;

    std::u16string formatted_omit_cryptographic_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(common_test.input)),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(common_test.output_omit_cryptographic_scheme,
              formatted_omit_cryptographic_scheme)
        << common_test.description;
  }

  const OriginTestData tests[] = {
      {"File URI", "file:///usr/example/file.html", u"file://", u"file://",
       u"file://"},
      {"File URI with hostname", "file://localhost/usr/example/file.html",
       u"file://localhost", u"file://localhost", u"file://localhost"},
      {"UNC File URI 1", "file:///CONTOSO/accounting/money.xls", u"file://",
       u"file://", u"file://"},
      {"UNC File URI 2",
       "file:///C:/Program%20Files/Music/Web%20Sys/main.html?REQUEST=RADIO",
       u"file://", u"file://", u"file://"},
      {"Invalid IPv6 address", "https://[2001:db8:0:1]/", u"", u"", u""},
      {"HTTP filesystem: URL with path",
       "filesystem:http://www.google.com/temporary/test.html",
       u"http://www.google.com", u"www.google.com", u"http://www.google.com"},
      {"File filesystem: URL with path",
       "filesystem:file://localhost/temporary/stuff/test.html?z=fun&goat=billy",
       u"file://", u"file://", u"file://"},
      {"Invalid scheme 1", "twelve://www.cyber.org/wow.php", u"", u"", u""},
      {"Invalid scheme 2", "://www.cyber.org/wow.php", u"", u"", u""},
      {"Invalid port 1", "https://173.194.65.103:99999", u"", u"", u""},
      {"Invalid port 2", "https://173.194.65.103:gruffle", u"", u"", u""},
      {"Blob URL",
       "blob:http://www.html5rocks.com/4d4ff040-6d61-4446-86d3-13ca07ec9ab9",
       u"http://www.html5rocks.com", u"www.html5rocks.com",
       u"http://www.html5rocks.com"}};

  for (const auto& test : tests) {
    std::u16string formatted = url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(GURL(test.input)));
    EXPECT_EQ(test.output, formatted) << test.description;

    std::u16string formatted_omit_web_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(test.input)),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    EXPECT_EQ(test.output_omit_web_scheme, formatted_omit_web_scheme)
        << test.description;

    std::u16string formatted_omit_cryptographic_scheme =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(GURL(test.input)),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    EXPECT_EQ(test.output_omit_cryptographic_scheme,
              formatted_omit_cryptographic_scheme)
        << test.description;
  }

  std::u16string formatted = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(GURL()));
  EXPECT_EQ(std::u16string(), formatted)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";

  std::u16string formatted_omit_scheme =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL()),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  EXPECT_EQ(std::u16string(), formatted_omit_scheme)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";

  formatted_omit_scheme = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(GURL()),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  EXPECT_EQ(std::u16string(), formatted_omit_scheme)
      << "Explicitly test the url::Origin which takes an empty, invalid URL";
}

TEST(TextEliderTest, FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains) {
  EXPECT_EQ(
      u"google.com",
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL("http://user:pass@google.com/path")));
  EXPECT_EQ(
      u"chrome://version",
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL("chrome://version")));
  EXPECT_EQ(
      u"äpple.de",
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL("https://äpple.de")));
  EXPECT_EQ(
      u"xn--pple-koa.com",
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL("https://äpple.com")));
  EXPECT_EQ(
      u"مثال.إختبار",
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL("https://xn--mgbh0fb.xn--kgbechtv/")));
}

TEST(TextEliderTest,
     FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix) {
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(
      u"google.com",
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              GURL("http://m.google.com/example")));
  EXPECT_EQ(
      u"google.com",
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              GURL("http://www.m.google.com/example")));
  EXPECT_EQ(
      u"google.com",
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              GURL("http://m.www.google.com/example")));
#else
  GTEST_SKIP();
#endif
}

}  // namespace
