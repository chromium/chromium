// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils.h"

#include "components/dom_distiller/core/url_constants.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace dom_distiller {

namespace url_utils {

TEST(DomDistillerUrlUtilsTest, TestPathUtil) {
  const std::string single_key = "mypath?foo=bar";
  EXPECT_EQ("bar", GetValueForKeyInUrlPathQuery(single_key, "foo"));

  const std::string two_keys = "mypath?key1=foo&key2=bar";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(two_keys, "key1"));
  EXPECT_EQ("bar", GetValueForKeyInUrlPathQuery(two_keys, "key2"));

  // First occurrence wins.
  const std::string multiple_same_key = "mypath?key=foo&key=bar";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(multiple_same_key, "key"));
}

TEST(DomDistillerUrlUtilsTest, TestGetValueForKeyInUrlPathQuery) {
  // Tests an invalid url.
  const std::string invalid_url = "http://%40[::1]/";
  EXPECT_EQ("", GetValueForKeyInUrlPathQuery(invalid_url, "key"));

  // Test a valid URL with the key we are searching for.
  const std::string valid_url_with_key = "http://www.google.com?key=foo";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(valid_url_with_key, "key"));

  // Test a valid URL without the key we are searching for.
  const std::string valid_url_no_key = "http://www.google.com";
  EXPECT_EQ("", GetValueForKeyInUrlPathQuery(valid_url_no_key, "key"));

  // Test a valid URL with 2 values of the key we are searching for.
  const std::string valid_url_two_keys =
      "http://www.google.com?key=foo&key=bar";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(valid_url_two_keys, "key"));
}

void AssertEqualExceptHost(const GURL& a, const GURL& b) {
  url::Replacements<char> no_host;
  no_host.ClearHost();
  EXPECT_EQ(a.ReplaceComponents(no_host), b.ReplaceComponents(no_host));
}

TEST(DomDistillerUrlUtilsTest, TestGetDistillerViewUrlFromUrl) {
  AssertEqualExceptHost(
      GURL("chrome-distiller://any/"
           "?title=cats&time=123&url=http%3A%2F%2Fexample.com%2Fpath%3Fq%3Dabc%"
           "26p%3D1%"
           "23anchor"),
      GetDistillerViewUrlFromUrl(
          kDomDistillerScheme, GURL("http://example.com/path?q=abc&p=1#anchor"),
          "cats", 123));
}

TEST(DomDistillerUrlUtilsTest, TestGetPageTitleFromDistillerUrl) {
  // Ensure that odd characters make it through.
  std::string title = "An Interesting Article: Cats >= Dogs!";
  GURL distilled = GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://example.com/path?q=abc&p=1#anchor"),
      title);
  EXPECT_EQ(title, GetTitleFromDistillerUrl(distilled));

  // Let's try some Unicode outside of BMP.
  title = "Γάτα قط ねこ חתול ␡";
  distilled = GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://example.com/article.html"), title);
  EXPECT_EQ(title, GetTitleFromDistillerUrl(distilled));
}

std::string GetOriginalUrlFromDistillerUrl(const std::string& url) {
  return GetOriginalUrlFromDistillerUrl(GURL(url)).spec();
}

TEST(DomDistillerUrlUtilsTest, TestGetOriginalUrlFromDistillerUrl) {
  EXPECT_EQ(
      "http://example.com/path?q=abc&p=1#anchor",
      GetOriginalUrlFromDistillerUrl(
          "chrome-distiller://"
          "any_"
          "d091ebf8f841eae9ca23822c3d0f369c16d3748478d0b74111be176eb96722e5/"
          "?time=123&url=http%3A%2F%2Fexample.com%2Fpath%3Fq%3Dabc%26p%3D1%"
          "23anchor"));
}

std::string ThroughDistiller(const std::string& url) {
  return GetOriginalUrlFromDistillerUrl(
             GetDistillerViewUrlFromUrl(kDomDistillerScheme, GURL(url), "title",
                                        123))
      .spec();
}

TEST(DomDistillerUrlUtilsTest, TestDistillerEndToEnd) {
  // Tests a normal url.
  const std::string url = "http://example.com/";
  EXPECT_EQ(url, ThroughDistiller(url));
  EXPECT_EQ(url, GetOriginalUrlFromDistillerUrl(url));

  // Tests a url with arguments and anchor points.
  const std::string url_arguments =
      "https://example.com/?key=value&key=value2&key2=value3#here";
  EXPECT_EQ(url_arguments, ThroughDistiller(url_arguments));
  EXPECT_EQ(url_arguments, GetOriginalUrlFromDistillerUrl(url_arguments));

  // Tests a url with file:// scheme.
  const std::string url_file = "file:///home/userid/path/index.html";
  EXPECT_EQ("", ThroughDistiller(url_file));
  EXPECT_EQ(url_file, GetOriginalUrlFromDistillerUrl(url_file));

  // Tests a nested url.
  const std::string nested_url =
      GetDistillerViewUrlFromUrl(kDomDistillerScheme, GURL(url), "title")
          .spec();
  EXPECT_EQ("", ThroughDistiller(nested_url));
  EXPECT_EQ(url, GetOriginalUrlFromDistillerUrl(nested_url));
}

TEST(DomDistillerUrlUtilsTest, TestRejectInvalidURLs) {
  const std::string url = "http://example.com/";
  const std::string url2 = "http://example.org/";
  const GURL view_url =
      GetDistillerViewUrlFromUrl(kDomDistillerScheme, GURL(url), "title", 123);
  GURL bad_view_url =
      net::AppendOrReplaceQueryParameter(view_url, kUrlKey, url2);
  EXPECT_EQ(GURL(), GetOriginalUrlFromDistillerUrl(bad_view_url));
}

TEST(DomDistillerUrlUtilsTest, TestRejectInvalidDistilledURLs) {
  EXPECT_FALSE(IsDistilledPage(GURL("chrome-distiller://any")));
  EXPECT_FALSE(IsDistilledPage(GURL("chrome-distiller://any/invalid")));
  EXPECT_FALSE(
      IsDistilledPage(GURL("chrome-distiller://any/?time=123&url=abc")));

  EXPECT_FALSE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      "not-distiller", GURL("http://example.com/"), "title")));
  EXPECT_FALSE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("not-http://example.com/"), "title")));

  EXPECT_TRUE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://example.com/"), "title")));
  EXPECT_TRUE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://www.example.com/page.html"), "title")));
  EXPECT_TRUE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      kDomDistillerScheme,
      GURL("http://www.example.com/page.html?cats=1&dogs=2"), "title")));
  EXPECT_TRUE(IsDistilledPage(GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("https://example.com/?params=any"), "title")));
}
}  // namespace url_utils

}  // namespace dom_distiller
