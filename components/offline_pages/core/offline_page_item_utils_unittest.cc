// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item_utils.h"

#include "components/offline_pages/core/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

TEST(OfflinePageItemUtils, EqualsIgnoringFragment) {
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                     GURL("http://example.com/")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                     GURL("http://example.com/#test")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/#test"),
                                     GURL("http://example.com/")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/#test"),
                                     GURL("http://example.com/#test2")));
  EXPECT_FALSE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                      GURL("http://test.com/#test")));
}

TEST(OfflinePageItemUtils, ClassifyOfflinePageUrlMatch_FinalUrl) {
  OfflinePageItem item;
  item.url = GURL("https://second.example.org/destination");

  EXPECT_EQ(OfflinePageUrlMatch::kMatchesFinalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("https://second.example.org/destination"), item));
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesFinalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("https://second.example.org/destination#section"), item));

  item.url = GURL("https://second.example.org/destination#saved");
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesFinalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("https://second.example.org/destination"), item));
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesFinalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("https://second.example.org/destination#other"), item));
}

TEST(OfflinePageItemUtils, ClassifyOfflinePageUrlMatch_OriginalUrl) {
  OfflinePageItem item;
  item.url = GURL("https://second.example.org/destination");
  item.original_url_if_different = GURL("http://first.example.com/redirect");

  EXPECT_EQ(OfflinePageUrlMatch::kMatchesOriginalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("http://first.example.com/redirect"), item));
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesOriginalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("http://first.example.com/redirect#section"), item));

  // Also matches when the saved original URL itself includes a fragment.
  item.original_url_if_different =
      GURL("http://first.example.com/redirect#initial");
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesOriginalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("http://first.example.com/redirect"), item));
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesOriginalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("http://first.example.com/redirect#different"), item));
}

TEST(OfflinePageItemUtils,
     ClassifyOfflinePageUrlMatch_FinalUrlTakesPrecedence) {
  OfflinePageItem item;
  item.url = GURL("https://example.com/page");
  item.original_url_if_different = GURL("https://example.com/page");

  EXPECT_EQ(
      OfflinePageUrlMatch::kMatchesFinalUrl,
      ClassifyOfflinePageUrlMatch(GURL("https://example.com/page"), item));
  EXPECT_EQ(
      OfflinePageUrlMatch::kMatchesFinalUrl,
      ClassifyOfflinePageUrlMatch(GURL("https://example.com/page#frag"), item));

  // If the redirect only changed the fragment, matching must still classify as
  // kMatchesFinalUrl so serving does not loop or strip the requested fragment.
  item.url = GURL("https://example.com/page#final");
  item.original_url_if_different = GURL("https://example.com/page#initial");
  EXPECT_EQ(
      OfflinePageUrlMatch::kMatchesFinalUrl,
      ClassifyOfflinePageUrlMatch(GURL("https://example.com/page#nav"), item));
}

TEST(OfflinePageItemUtils, ClassifyOfflinePageUrlMatch_MismatchAndInvalidUrls) {
  OfflinePageItem item;
  item.url = GURL("https://second.example.org/destination");
  item.original_url_if_different = GURL("http://first.example.com/redirect");

  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL("http://first.example.com/other"),
                                        item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL(), item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL("http://"), item));

  // Non-empty invalid URLs on the item must never match valid or invalid
  // request URLs.
  OfflinePageItem invalid_item;
  invalid_item.url = GURL("https://");
  invalid_item.original_url_if_different = GURL("http://");
  ASSERT_FALSE(invalid_item.url.is_empty());
  ASSERT_FALSE(invalid_item.url.is_valid());
  EXPECT_EQ(
      OfflinePageUrlMatch::kNone,
      ClassifyOfflinePageUrlMatch(GURL("https://example.com/"), invalid_item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL("https://"), invalid_item));
}

TEST(OfflinePageItemUtils, ClassifyOfflinePageUrlMatch_NonHttpOrHttpsSchemes) {
  // Valid non-HTTP/HTTPS request URLs must never match even if stored on the
  // item.
  OfflinePageItem non_http_item;
  non_http_item.url = GURL("chrome://version");
  non_http_item.original_url_if_different = GURL("file:///tmp/page.mhtml");
  EXPECT_EQ(
      OfflinePageUrlMatch::kNone,
      ClassifyOfflinePageUrlMatch(GURL("chrome://version"), non_http_item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL("file:///tmp/page.mhtml"),
                                        non_http_item));

  // A corrupted item with a non-HTTP/HTTPS final URL must not match a valid
  // HTTP/HTTPS original URL, preventing callers like OfflinePageTabHelper from
  // navigating to `item.url` or letting the row consume maximum_matches slots.
  OfflinePageItem corrupted_final_item;
  corrupted_final_item.url = GURL("chrome://version");
  corrupted_final_item.original_url_if_different =
      GURL("http://first.example.com/redirect");
  EXPECT_EQ(
      OfflinePageUrlMatch::kNone,
      ClassifyOfflinePageUrlMatch(GURL("http://first.example.com/redirect"),
                                  corrupted_final_item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(
                GURL("http://first.example.com/redirect#frag"),
                corrupted_final_item));

  // When `item.url` is valid HTTP/HTTPS and only `original_url_if_different`
  // has a non-HTTP/HTTPS scheme, `item.url` still matches as kMatchesFinalUrl
  // while `original_url_if_different` never matches.
  OfflinePageItem corrupted_original_item;
  corrupted_original_item.url = GURL("https://second.example.org/destination");
  corrupted_original_item.original_url_if_different =
      GURL("file:///tmp/redirect.mhtml");
  EXPECT_EQ(OfflinePageUrlMatch::kMatchesFinalUrl,
            ClassifyOfflinePageUrlMatch(
                GURL("https://second.example.org/destination#frag"),
                corrupted_original_item));
  EXPECT_EQ(OfflinePageUrlMatch::kNone,
            ClassifyOfflinePageUrlMatch(GURL("file:///tmp/redirect.mhtml"),
                                        corrupted_original_item));
}

}  // namespace
}  // namespace offline_pages
