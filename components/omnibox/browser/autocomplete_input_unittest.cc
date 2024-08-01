// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/autocomplete_input.h"

#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/third_party/mozilla/url_parse.h"

using base::ASCIIToUTF16;
using metrics::OmniboxEventProto;

TEST(AutocompleteInputTest, InputType) {
  struct test_data {
    const std::u16string input;
    const metrics::OmniboxInputType type;
  } input_cases[] = {
    {std::u16string(), metrics::OmniboxInputType::EMPTY},
    {u"?", metrics::OmniboxInputType::QUERY},
    {u"?foo", metrics::OmniboxInputType::QUERY},
    {u"?foo bar", metrics::OmniboxInputType::QUERY},
    {u"?http://foo.com/bar", metrics::OmniboxInputType::QUERY},
    {u"foo", metrics::OmniboxInputType::UNKNOWN},
    {u"foo._", metrics::OmniboxInputType::QUERY},
    {u"foo.c", metrics::OmniboxInputType::UNKNOWN},
    {u"foo.com", metrics::OmniboxInputType::URL},
    {u"-foo.com", metrics::OmniboxInputType::URL},
    {u"foo-.com", metrics::OmniboxInputType::URL},
    {u"foo_.com", metrics::OmniboxInputType::URL},
    {u"foo.-com", metrics::OmniboxInputType::QUERY},
    {u"foo/", metrics::OmniboxInputType::URL},
    {u"foo/bar", metrics::OmniboxInputType::UNKNOWN},
    {u"foo/bar%00", metrics::OmniboxInputType::UNKNOWN},
    {u"foo/bar/", metrics::OmniboxInputType::URL},
    {u"foo/bar baz\\", metrics::OmniboxInputType::URL},
    {u"foo.com/bar", metrics::OmniboxInputType::URL},
    {u"foo;bar", metrics::OmniboxInputType::QUERY},
    {u"foo/bar baz", metrics::OmniboxInputType::UNKNOWN},
    {u"foo bar.com", metrics::OmniboxInputType::QUERY},
    {u"foo bar", metrics::OmniboxInputType::QUERY},
    {u"foo+bar", metrics::OmniboxInputType::QUERY},
    {u"foo+bar.com", metrics::OmniboxInputType::UNKNOWN},
    {u"\"foo:bar\"", metrics::OmniboxInputType::QUERY},
    {u"link:foo.com", metrics::OmniboxInputType::UNKNOWN},
    {u"foo:81", metrics::OmniboxInputType::URL},
    {u"www.foo.com:81", metrics::OmniboxInputType::URL},
    {u"foo.com:123456", metrics::OmniboxInputType::QUERY},
    {u"foo.com:abc", metrics::OmniboxInputType::QUERY},
    {u"1.2.3.4:abc", metrics::OmniboxInputType::QUERY},
    {u"user@foo", metrics::OmniboxInputType::UNKNOWN},
    {u"user@foo.com", metrics::OmniboxInputType::UNKNOWN},
    {u"user@foo/", metrics::OmniboxInputType::URL},
    {u"user@foo/z", metrics::OmniboxInputType::URL},
    {u"user@foo/z z", metrics::OmniboxInputType::URL},
    {u"user@foo.com/z", metrics::OmniboxInputType::URL},
    {u"user @foo/", metrics::OmniboxInputType::UNKNOWN},
    {u"us er@foo/z", metrics::OmniboxInputType::UNKNOWN},
    {u"u ser@foo/z z", metrics::OmniboxInputType::UNKNOWN},
    {u"us er@foo.com/z", metrics::OmniboxInputType::UNKNOWN},
    {u"user:pass@", metrics::OmniboxInputType::UNKNOWN},
    {u"user:pass@!foo.com", metrics::OmniboxInputType::UNKNOWN},
    {u"user:pass@foo", metrics::OmniboxInputType::URL},
    {u"user:pass@foo.c", metrics::OmniboxInputType::URL},
    {u"user:pass@foo.com", metrics::OmniboxInputType::URL},
    {u"space user:pass@foo", metrics::OmniboxInputType::UNKNOWN},
    {u"space user:pass@foo.c", metrics::OmniboxInputType::UNKNOWN},
    {u"space user:pass@foo.com", metrics::OmniboxInputType::UNKNOWN},
    {u"user:pass@foo.com:81", metrics::OmniboxInputType::URL},
    {u"user:pass@foo:81", metrics::OmniboxInputType::URL},
    {u".1", metrics::OmniboxInputType::QUERY},
    {u".1/3", metrics::OmniboxInputType::QUERY},
    {u"1.2", metrics::OmniboxInputType::QUERY},
    {u".1.2", metrics::OmniboxInputType::UNKNOWN},
    {u"1.2/", metrics::OmniboxInputType::URL},
    {u"1.2/45", metrics::OmniboxInputType::QUERY},
    {u"6008/32768", metrics::OmniboxInputType::QUERY},
    {u"12345678/", metrics::OmniboxInputType::QUERY},
    {u"123456789/", metrics::OmniboxInputType::URL},
    {u"1.2:45", metrics::OmniboxInputType::QUERY},
    {u"user@1.2:45", metrics::OmniboxInputType::QUERY},
    {u"user@foo:45", metrics::OmniboxInputType::URL},
    {u"user:pass@1.2:45", metrics::OmniboxInputType::URL},
    {u"host?query", metrics::OmniboxInputType::UNKNOWN},
    {u"host#", metrics::OmniboxInputType::UNKNOWN},
    {u"host#ref", metrics::OmniboxInputType::UNKNOWN},
    {u"host# ref", metrics::OmniboxInputType::UNKNOWN},
    {u"host/page.html", metrics::OmniboxInputType::UNKNOWN},
    {u"host/#ref", metrics::OmniboxInputType::URL},
    {u"host/?#ref", metrics::OmniboxInputType::URL},
    {u"host/?#", metrics::OmniboxInputType::URL},
    {u"host.com#ref", metrics::OmniboxInputType::URL},
    {u"http://host#ref", metrics::OmniboxInputType::URL},
    {u"host/path?query", metrics::OmniboxInputType::URL},
    {u"host/path#ref", metrics::OmniboxInputType::URL},
    {u"en.wikipedia.org/wiki/Jim Beam", metrics::OmniboxInputType::URL},
    // In Chrome itself, mailto: will get handled by ShellExecute, but in
    // unittest mode, we don't have the data loaded in the external protocol
    // handler to know this.
    // { u"mailto:abuse@foo.com", metrics::OmniboxInputType::URL },
    {u"view-source:http://www.foo.com/", metrics::OmniboxInputType::URL},
    {u"javascript", metrics::OmniboxInputType::UNKNOWN},
    {u"javascript:alert(\"Hi there\");", metrics::OmniboxInputType::URL},
    {u"javascript:alert%28\"Hi there\"%29;", metrics::OmniboxInputType::URL},
    {u"javascript:foo", metrics::OmniboxInputType::UNKNOWN},
    {u"javascript:foo;", metrics::OmniboxInputType::URL},
    {u"javascript:\"foo\"", metrics::OmniboxInputType::URL},
    {u"javascript:", metrics::OmniboxInputType::UNKNOWN},
    {u"javascript:the cromulent parts", metrics::OmniboxInputType::UNKNOWN},
    {u"javascript:foo.getter", metrics::OmniboxInputType::URL},
    {u"JavaScript:Tutorials", metrics::OmniboxInputType::UNKNOWN},
#if BUILDFLAG(IS_WIN)
    {u"C:\\Program Files", metrics::OmniboxInputType::URL},
    {u"\\\\Server\\Folder\\File", metrics::OmniboxInputType::URL},
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    {u"file:///foo", metrics::OmniboxInputType::QUERY},
    {u"/foo", metrics::OmniboxInputType::QUERY},
#else
    {u"file:///foo", metrics::OmniboxInputType::URL},
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    {u"http:foo", metrics::OmniboxInputType::URL},
    {u"http://foo", metrics::OmniboxInputType::URL},
    {u"http://foo._", metrics::OmniboxInputType::UNKNOWN},
    {u"http://foo.c", metrics::OmniboxInputType::URL},
    {u"http://foo.com", metrics::OmniboxInputType::URL},
    {u"http://foo_bar.com", metrics::OmniboxInputType::URL},
    {u"http://foo/bar%00", metrics::OmniboxInputType::URL},
    {u"http://foo/bar baz", metrics::OmniboxInputType::URL},
    {u"http://-foo.com", metrics::OmniboxInputType::URL},
    {u"http://foo-.com", metrics::OmniboxInputType::URL},
    {u"http://foo_.com", metrics::OmniboxInputType::URL},
    {u"http://foo.-com", metrics::OmniboxInputType::UNKNOWN},
    {u"http://_foo_.com", metrics::OmniboxInputType::URL},
    {u"http://foo.com:abc", metrics::OmniboxInputType::QUERY},
    {u"http://foo.com:123456", metrics::OmniboxInputType::QUERY},
    {u"http://1.2.3.4:abc", metrics::OmniboxInputType::QUERY},
    {u"http:user@foo.com", metrics::OmniboxInputType::URL},
    {u"http://user@foo.com", metrics::OmniboxInputType::URL},
    {u"http://space user@foo.com", metrics::OmniboxInputType::URL},
    {u"http://user:pass@foo", metrics::OmniboxInputType::URL},
    {u"http://space user:pass@foo", metrics::OmniboxInputType::URL},
    {u"http:space user:pass@foo", metrics::OmniboxInputType::URL},
    {u"http:user:pass@foo.com", metrics::OmniboxInputType::URL},
    {u"http://user:pass@foo.com", metrics::OmniboxInputType::URL},
    {u"http://1.2", metrics::OmniboxInputType::URL},
    {u"http:user@1.2", metrics::OmniboxInputType::URL},
    {u"http://1.2/45", metrics::OmniboxInputType::URL},
    {u"http:ps/2 games", metrics::OmniboxInputType::URL},
    {u"https://foo.com", metrics::OmniboxInputType::URL},
    {u"127.0.0.1", metrics::OmniboxInputType::URL},
    {u"127.0.1", metrics::OmniboxInputType::QUERY},
    {u"127.0.1/", metrics::OmniboxInputType::URL},
    {u"0.0.0", metrics::OmniboxInputType::QUERY},
    {u"0.0.0.0", metrics::OmniboxInputType::URL},
    {u"0.0.0.1", metrics::OmniboxInputType::QUERY},
    {u"http://0.0.0.1/", metrics::OmniboxInputType::QUERY},
    {u"browser.tabs.closeButtons", metrics::OmniboxInputType::UNKNOWN},
    {u"\u6d4b\u8bd5", metrics::OmniboxInputType::UNKNOWN},
    {u"[2001:]", metrics::OmniboxInputType::QUERY},
    {u"[2001:dB8::1]", metrics::OmniboxInputType::URL},
    {u"192.168.0.256", metrics::OmniboxInputType::QUERY},
    {u"[foo.com]", metrics::OmniboxInputType::QUERY},
    {u"filesystem:http://a.com/t/bar", metrics::OmniboxInputType::URL},
    {u"filesystem:http://a.com/", metrics::OmniboxInputType::QUERY},
    {u"filesystem:file://", metrics::OmniboxInputType::QUERY},
    {u"filesystem:http", metrics::OmniboxInputType::QUERY},
    {u"filesystem:", metrics::OmniboxInputType::QUERY},
    {u"chrome-search://", metrics::OmniboxInputType::QUERY},
    {u"chrome-devtools:", metrics::OmniboxInputType::UNKNOWN},
    {u"chrome-devtools://", metrics::OmniboxInputType::UNKNOWN},
    {u"chrome-devtools://x", metrics::OmniboxInputType::UNKNOWN},
    {u"devtools:", metrics::OmniboxInputType::QUERY},
    {u"devtools://", metrics::OmniboxInputType::QUERY},
    {u"devtools://x", metrics::OmniboxInputType::URL},
    {u"about://f;", metrics::OmniboxInputType::URL},
    {u"://w", metrics::OmniboxInputType::UNKNOWN},
    {u":w", metrics::OmniboxInputType::UNKNOWN},
    {u".\u062A", metrics::OmniboxInputType::UNKNOWN},
    // These tests are for https://tools.ietf.org/html/rfc6761.
    {u"localhost", metrics::OmniboxInputType::URL},
    {u"localhost:8080", metrics::OmniboxInputType::URL},
    {u"foo.localhost", metrics::OmniboxInputType::URL},
    {u"foo localhost", metrics::OmniboxInputType::QUERY},
    {u"foo.example", metrics::OmniboxInputType::URL},
    {u"foo example", metrics::OmniboxInputType::QUERY},
    {u"http://example/", metrics::OmniboxInputType::URL},
    {u"example", metrics::OmniboxInputType::UNKNOWN},
    {u"example ", metrics::OmniboxInputType::UNKNOWN},
    {u" example", metrics::OmniboxInputType::UNKNOWN},
    {u" example ", metrics::OmniboxInputType::UNKNOWN},
    {u"example.", metrics::OmniboxInputType::UNKNOWN},
    {u".example", metrics::OmniboxInputType::UNKNOWN},
    {u".example.", metrics::OmniboxInputType::UNKNOWN},
    {u"example:", metrics::OmniboxInputType::UNKNOWN},
    {u"example:80/ ", metrics::OmniboxInputType::URL},
    {u"http://foo.invalid", metrics::OmniboxInputType::UNKNOWN},
    {u"foo.invalid/", metrics::OmniboxInputType::QUERY},
    {u"foo.invalid", metrics::OmniboxInputType::QUERY},
    {u"foo invalid", metrics::OmniboxInputType::QUERY},
    {u"invalid", metrics::OmniboxInputType::UNKNOWN},
    {u"foo.test", metrics::OmniboxInputType::URL},
    {u"foo test", metrics::OmniboxInputType::QUERY},
    {u"test", metrics::OmniboxInputType::UNKNOWN},
    {u"test..", metrics::OmniboxInputType::UNKNOWN},
    {u"..test", metrics::OmniboxInputType::UNKNOWN},
    {u"test:80/", metrics::OmniboxInputType::URL},
    {u"foo.local", metrics::OmniboxInputType::URL},
    {u"foo local", metrics::OmniboxInputType::QUERY},
    {u"local", metrics::OmniboxInputType::UNKNOWN},
    {u".local", metrics::OmniboxInputType::UNKNOWN},
  };

  for (size_t i = 0; i < std::size(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input,
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_prevent_inline_autocomplete(true);
    EXPECT_EQ(input_cases[i].type, input.type());
  }
}

TEST(AutocompleteInputTest, InputTypeWithDesiredTLD) {
  struct test_data {
    const std::u16string input;
    const metrics::OmniboxInputType type;
    const std::string spec;  // Unused if not a URL.
  } input_cases[] = {
      {u"401k", metrics::OmniboxInputType::URL,
       std::string("http://www.401k.com/")},
      {u"56", metrics::OmniboxInputType::URL,
       std::string("http://www.56.com/")},
      {u"1.2", metrics::OmniboxInputType::URL,
       std::string("http://www.1.2.com/")},
      {u"1.2/3.4", metrics::OmniboxInputType::URL,
       std::string("http://www.1.2.com/3.4")},
      {u"192.168.0.1", metrics::OmniboxInputType::URL,
       std::string("http://www.192.168.0.1.com/")},
      {u"999999999999999", metrics::OmniboxInputType::URL,
       std::string("http://www.999999999999999.com/")},
      {u"x@y", metrics::OmniboxInputType::URL,
       std::string("http://x@www.y.com/")},
      {u"x@y.com", metrics::OmniboxInputType::URL,
       std::string("http://x@y.com/")},
      {u"space user@y", metrics::OmniboxInputType::UNKNOWN, std::string()},
      {u"y/z z", metrics::OmniboxInputType::URL,
       std::string("http://www.y.com/z%20z")},
      {u"abc.com", metrics::OmniboxInputType::URL,
       std::string("http://abc.com/")},
      {u"foo bar", metrics::OmniboxInputType::QUERY, std::string()},
  };

  for (size_t i = 0; i < std::size(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, std::u16string::npos, "com",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_prevent_inline_autocomplete(true);
    EXPECT_EQ(input_cases[i].type, input.type());
    if (input_cases[i].type == metrics::OmniboxInputType::URL)
      EXPECT_EQ(input_cases[i].spec, input.canonicalized_url().spec());
  }
}

// This tests for a regression where certain input in the omnibox caused us to
// crash. As long as the test completes without crashing, we're fine.
TEST(AutocompleteInputTest, InputCrash) {
  AutocompleteInput input(u"\uff65@s", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  // Not strictly necessary, but let's be thorough.
  input.set_prevent_inline_autocomplete(true);
}

TEST(AutocompleteInputTest, ParseForEmphasizeComponent) {
  using url::Component;
  Component kInvalidComponent(0, -1);
  struct test_data {
    const std::u16string input;
    const Component scheme;
    const Component host;
  } input_cases[] = {
      {std::u16string(), kInvalidComponent, kInvalidComponent},
      {u"?", kInvalidComponent, kInvalidComponent},
      {u"?http://foo.com/bar", kInvalidComponent, kInvalidComponent},
      {u"foo/bar baz", kInvalidComponent, Component(0, 3)},
      {u"http://foo/bar baz", Component(0, 4), Component(7, 3)},
      {u"link:foo.com", Component(0, 4), kInvalidComponent},
      {u"www.foo.com:81", kInvalidComponent, Component(0, 11)},
      {u"\u6d4b\u8bd5", kInvalidComponent, Component(0, 2)},
      {u"view-source:http://www.foo.com/", Component(12, 4), Component(19, 11)},
      {u"view-source:https://example.com/", Component(12, 5),
       Component(20, 11)},
      {u"view-source:www.foo.com", kInvalidComponent, Component(12, 11)},
      {u"view-source:", Component(0, 11), kInvalidComponent},
      {u"view-source:garbage", kInvalidComponent, Component(12, 7)},
      {u"view-source:http://http://foo", Component(12, 4), Component(19, 4)},
      {u"view-source:view-source:http://example.com/", Component(12, 11),
       kInvalidComponent},
      {u"blob:http://www.foo.com/", Component(5, 4), Component(12, 11)},
      {u"blob:https://example.com/", Component(5, 5), Component(13, 11)},
      {u"blob:www.foo.com", kInvalidComponent, Component(5, 11)},
      {u"blob:", Component(0, 4), kInvalidComponent},
      {u"blob:garbage", kInvalidComponent, Component(5, 7)},
  };

  for (size_t i = 0; i < std::size(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    Component scheme, host;
    AutocompleteInput::ParseForEmphasizeComponents(input_cases[i].input,
                                                   TestSchemeClassifier(),
                                                   &scheme,
                                                   &host);
    EXPECT_EQ(input_cases[i].scheme.begin, scheme.begin);
    EXPECT_EQ(input_cases[i].scheme.len, scheme.len);
    EXPECT_EQ(input_cases[i].host.begin, host.begin);
    EXPECT_EQ(input_cases[i].host.len, host.len);
  }
}

TEST(AutocompleteInputTest, InputTypeWithCursorPosition) {
  struct test_data {
    const std::u16string input;
    size_t cursor_position;
    const std::u16string normalized_input;
    size_t normalized_cursor_position;
  } input_cases[] = {
      {u"foo bar", std::u16string::npos, u"foo bar", std::u16string::npos},

      // Regular case, no changes.
      {u"foo bar", 3, u"foo bar", 3},

      // Extra leading space.
      {u"  foo bar", 3, u"foo bar", 1},
      {u"      foo bar", 3, u"foo bar", 0},
      {u"      foo bar   ", 2, u"foo bar   ", 0},

      // A leading '?' used to be a magic character indicating the following
      // input should be treated as a "forced query", but now if such a string
      // reaches the AutocompleteInput parser the '?' should just be treated
      // like a normal character.
      {u"?foo bar", 2, u"?foo bar", 2},
      {u"  ?foo bar", 4, u"?foo bar", 2},
      {u"?  foo bar", 4, u"?  foo bar", 4},
      {u"  ?  foo bar", 6, u"?  foo bar", 4},
  };

  for (size_t i = 0; i < std::size(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(
        input_cases[i].input, input_cases[i].cursor_position,
        metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    input.set_prevent_inline_autocomplete(true);
    EXPECT_EQ(input_cases[i].normalized_input, input.text());
    EXPECT_EQ(input_cases[i].normalized_cursor_position,
              input.cursor_position());
  }
}

TEST(AutocompleteInputTest, UpgradeTypedNavigationsToHttps) {
  struct TestData {
    const std::u16string input;
    const GURL expected_url;
    bool expected_added_default_scheme_to_typed_url;
  };

  const TestData test_cases[] = {
      {u"example.com", GURL("https://example.com"), true},
      // If the hostname has a port specified, the URL shouldn't be upgraded
      // to HTTPS because we can't assume that the HTTPS site is served over the
      // default SSL port. Port 80 is dropped in URLs so it's still upgraded.
      {u"example.com:80", GURL("https://example.com"), true},
      {u"example.com:8080", GURL("http://example.com:8080"), false},
      // Non-URL inputs shouldn't be upgraded.
      {u"example query", GURL(), false},
      // IP addresses shouldn't be upgraded.
      {u"127.0.0.1", GURL("http://127.0.0.1"), false},
      {u"127.0.0.1:80", GURL("http://127.0.0.1:80"), false},
      {u"127.0.0.1:8080", GURL("http://127.0.0.1:8080"), false},
      // Non-unique hostnames shouldn't be upgraded.
      {u"site.test", GURL("http://site.test"), false},
      // This non-unique hostname is a regression test for
      // https://crbug.com/1224724. The slash is provided at the end of the
      // input query since otherwise the input gets classified as a non-URL and
      // the autocomplete code doesn't progress to the HTTPS upgrading logic
      // where the bug was.
      {u"dotlesshostname/", GURL("http://dotlesshostname/"), false},
      {u"http://dotlesshostname/", GURL("http://dotlesshostname/"), false},
      {u"https://dotlesshostname/", GURL("https://dotlesshostname/"), false},
      // Fully typed URLs shouldn't be upgraded.
      {u"http://example.com", GURL("http://example.com"), false},
      {u"HTTP://EXAMPLE.COM", GURL("http://example.com"), false},
      {u"http://example.com:80", GURL("http://example.com"), false},
      {u"HTTP://EXAMPLE.COM:80", GURL("http://example.com"), false},
      {u"http://example.com:8080", GURL("http://example.com:8080"), false},
      {u"HTTP://EXAMPLE.COM:8080", GURL("http://example.com:8080"), false},
  };
  for (const TestData& test_case : test_cases) {
    AutocompleteInput input(test_case.input, std::u16string::npos,
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier(),
                            /*should_use_https_as_default_scheme=*/true);
    EXPECT_EQ(test_case.expected_url, input.canonicalized_url())
        << test_case.input;
    EXPECT_EQ(test_case.expected_added_default_scheme_to_typed_url,
              input.added_default_scheme_to_typed_url());
  }

  // Try the same test cases with a non-zero HTTPS port passed to
  // AutocompleteInput. When a non-zero HTTPS port is used, AutoCompleteInput
  // should use that port to replace the port of the HTTP URL when upgrading
  // the URL.
  // We don't check the default port 80 being upgraded in these test case,
  // because the default port will be dropped by GURL and we'll end up with
  // example.com. A hostname without a port is not a valid input when using a
  // non-zero value for https_port_for_testing.
  int https_port_for_testing = 12345;
  const TestData test_cases_non_default_port[] = {
    {u"example.com:8080", GURL("https://example.com:12345"), true},
    // Non-URL inputs shouldn't be upgraded.
    {u"example query", GURL(), false},
    // Non-unique hostnames shouldn't be upgraded.
    {u"site.test", GURL("http://site.test"), false},

#if !BUILDFLAG(IS_IOS)
    // IP addresses shouldn't be upgraded.
    {u"127.0.0.1", GURL("http://127.0.0.1"), false},
    {u"127.0.0.1:80", GURL("http://127.0.0.1:80"), false},
    {u"127.0.0.1:8080", GURL("http://127.0.0.1:8080"), false},
#else
    // On iOS, IP addresses will be upgraded in tests if the hostname has a
    // non-default port.
    {u"127.0.0.1:8080", GURL("https://127.0.0.1:12345"), true},
#endif
    //
    // Fully typed URLs shouldn't be upgraded.
    {u"http://example.com", GURL("http://example.com"), false},
    {u"HTTP://EXAMPLE.COM", GURL("http://example.com"), false},
    {u"http://example.com:80", GURL("http://example.com"), false},
    {u"HTTP://EXAMPLE.COM:80", GURL("http://example.com"), false},
    {u"http://example.com:8080", GURL("http://example.com:8080"), false},
    {u"HTTP://EXAMPLE.COM:8080", GURL("http://example.com:8080"), false}
  };
  for (const TestData& test_case : test_cases_non_default_port) {
    AutocompleteInput input(
        test_case.input, std::u16string::npos,
        metrics::OmniboxEventProto::OTHER, TestSchemeClassifier(),
        /*should_use_https_as_default_scheme=*/true, https_port_for_testing);
    EXPECT_EQ(test_case.expected_url, input.canonicalized_url())
        << test_case.input;
    EXPECT_EQ(test_case.expected_added_default_scheme_to_typed_url,
              input.added_default_scheme_to_typed_url());
  }

#if BUILDFLAG(IS_IOS)
  AutocompleteInput fake_http_input(
      u"127.0.0.1:8080", std::u16string::npos,
      metrics::OmniboxEventProto::OTHER, TestSchemeClassifier(),
      /*should_use_https_as_default_scheme=*/true,
      /*https_port_for_testing=*/12345,
      /*use_fake_https_for_https_upgrade_testing=*/true);
  EXPECT_EQ(GURL("http://127.0.0.1:12345"),
            fake_http_input.canonicalized_url());
  EXPECT_TRUE(fake_http_input.added_default_scheme_to_typed_url());
#endif
}

TEST(AutocompleteInputTest, TypedURLHadHTTPSchemeTest) {
  struct TestData {
    const std::u16string input;
    bool expected_typed_url_had_http_scheme;
  };

  const TestData test_cases[] = {
      {u"example.com", false},
      {u"example.com:80", false},
      {u"example.com:8080", false},
      {u"example query", false},
      {u"http example query", false},
      {u"127.0.0.1", false},
      {u"127.0.0.1:80", false},
      {u"127.0.0.1:8080", false},
      {u"http://127.0.0.1:8080", true},
      {u"https://127.0.0.1:8080", false},
      {u"dotlesshostname/", false},
      {u"http://dotlesshostname/", true},
      {u"https://dotlesshostname/", false},
      {u"http://example.com", true},
      {u"HTTP://EXAMPLE.COM", true},
      {u"http://example.com:80", true},
      {u"HTTP://EXAMPLE.COM:80", true},
      {u"http://example.com:8080", true},
      {u"HTTP://EXAMPLE.COM:8080", true},
      {u"HTTPS://EXAMPLE.COM", false},
  };
  for (const TestData& test_case : test_cases) {
    AutocompleteInput input(test_case.input, std::u16string::npos,
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier(),
                            /*should_use_https_as_default_scheme=*/true);
    EXPECT_EQ(test_case.expected_typed_url_had_http_scheme,
              input.typed_url_had_http_scheme());
  }
}
