// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

// See additional test coverage for edge cases in GURLTest.GetWithoutFilename.
TEST(BookmarkAppUtil, IsInNavigationScopeForLaunchUrl_UrlArgumentVariations) {
  const GURL launch_url = GURL("https://mail.google.com/mail/u/0");

  // Not in scope.
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com/mail/")));

  // The scope itself.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com/mail/u/")));
  // No match if no trailing '/' in path.
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com/mail/u")));

  // Regular sub-path.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com/mail/u/0/")));

  // With a ref.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      launch_url, GURL("https://mail.google.com/mail/u/0/#inbox")));

  // A launch URL with trailing '/' resolves to itself.
  const GURL launch_url2 = GURL("https://example.com/path/subpath/");
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      launch_url2, GURL("https://example.com/path/subpath/page.html")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      launch_url2, GURL("https://example.com/path/subpath2/")));

  // With a query.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/subpath"),
      GURL("https://example.com/path/query?parameter")));
}

TEST(BookmarkAppUtil, IsInNavigationScopeForLaunchUrl_LaunchUrlVariations) {
  // With a query.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/query?parameter"),
      GURL("https://example.com/path/subpath")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/query?parameter"),
      GURL("https://example.com/query")));

  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/query/?parameter"),
      GURL("https://example.com/path/query/")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/query/?parameter"),
      GURL("https://example.com/path/subpath/")));

  // With a ref.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/#ref"),
      GURL("https://example.com/path/subpath")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/#ref"), GURL("https://example.com/#ref")));

  // With a ref and query.
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/?query=param#ref"),
      GURL("https://example.com/path/subpath")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/?query=param#ref"),
      GURL("https://example.com/subpath")));

  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/#ref?query=param"),
      GURL("https://example.com/path/subpath")));
  EXPECT_FALSE(IsInNavigationScopeForLaunchUrl(
      GURL("https://example.com/path/#ref?query=param"),
      GURL("https://example.com/subpath")));
}

TEST(BookmarkAppUtil, IsInNavigationScopeForLaunchUrl_Extensions) {
  // The Crosh extension.
  const GURL extension_launch_url = GURL(
      "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/html/crosh.html");
  EXPECT_TRUE(IsInNavigationScopeForLaunchUrl(
      extension_launch_url,
      GURL("chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/html/path")));
}

}  // namespace extensions
