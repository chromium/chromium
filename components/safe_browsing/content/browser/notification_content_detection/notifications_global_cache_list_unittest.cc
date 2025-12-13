// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kNotificationsGlobalCacheListSize[] =
    "SafeBrowsing.NotificationsGlobalCacheList.Size";
const char kNotificationsGlobalCacheListOriginIsListed[] =
    "SafeBrowsing.NotificationsGlobalCacheList.OriginIsListed";

}  // namespace

namespace safe_browsing {

using NotificationsGlobalCacheListTest = testing::Test;

TEST_F(NotificationsGlobalCacheListTest, SkipDomainIfInListOrEmptyList) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(
      GURL("https://google.com")));
  EXPECT_TRUE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(
      GURL("https://google.com/foo/bar.html")));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Cache list size histogram should be logged twice and the value should not
  // be 0.
  histogram_tester.ExpectTotalCount(kNotificationsGlobalCacheListSize, 2);
  histogram_tester.ExpectBucketCount(kNotificationsGlobalCacheListSize, 0, 0);
  // Log true twice, since the origins are in the list.
  histogram_tester.ExpectUniqueSample(
      kNotificationsGlobalCacheListOriginIsListed, true, 2);
#else
  // Cache list should have no entries.
  histogram_tester.ExpectUniqueSample(kNotificationsGlobalCacheListSize, 0, 2);
  // Don't log anything, since the cache list has no entries.
  histogram_tester.ExpectTotalCount(kNotificationsGlobalCacheListOriginIsListed,
                                    0);
#endif
}

TEST_F(NotificationsGlobalCacheListTest, InvalidURL) {
  base::HistogramTester histogram_tester;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_FALSE(
      ShouldSkipNotificationProtectionsDueToGlobalCacheList(GURL("not-a-url")));
  EXPECT_FALSE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(GURL("")));
  // Cache list size histogram should be logged twice and the value should not
  // be 0.
  histogram_tester.ExpectTotalCount(kNotificationsGlobalCacheListSize, 2);
  histogram_tester.ExpectBucketCount(kNotificationsGlobalCacheListSize, 0, 0);
  // Log false twice, since the origin is in the list.
  histogram_tester.ExpectUniqueSample(
      kNotificationsGlobalCacheListOriginIsListed, false, 2);
#else
  EXPECT_TRUE(
      ShouldSkipNotificationProtectionsDueToGlobalCacheList(GURL("not-a-url")));
  EXPECT_TRUE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(GURL("")));
  // Cache list should have no entries for both logs.
  histogram_tester.ExpectUniqueSample(kNotificationsGlobalCacheListSize, 0, 2);
  // Don't log anything, since the cache list has no entries.
  histogram_tester.ExpectTotalCount(kNotificationsGlobalCacheListOriginIsListed,
                                    0);
#endif
}

TEST_F(NotificationsGlobalCacheListTest, GetAndSetDomains) {
  std::vector<std::string> domains = {"one.com", "two.com", "three.com"};
  SetNotificationsGlobalCacheListDomainsForTesting(domains);
  EXPECT_EQ(GetNotificationsGlobalCacheListDomains(), domains);

  SetNotificationsGlobalCacheListDomainsForTesting({});
  EXPECT_TRUE(GetNotificationsGlobalCacheListDomains().empty());
}

TEST_F(NotificationsGlobalCacheListTest,
       EmptyListShouldSkipNotificationProtections) {
  base::HistogramTester histogram_tester;
  SetNotificationsGlobalCacheListDomainsForTesting({});
  EXPECT_TRUE(GetNotificationsGlobalCacheListDomains().empty());
  EXPECT_TRUE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(
      GURL("https://google.com")));
  EXPECT_TRUE(ShouldSkipNotificationProtectionsDueToGlobalCacheList(
      GURL("https://one.com")));
  // Cache list size histogram should be logged twice and the value should be 0.
  histogram_tester.ExpectBucketCount(kNotificationsGlobalCacheListSize, 0, 2);
  // Don't log anything, since the cache list has no entries.
  histogram_tester.ExpectTotalCount(kNotificationsGlobalCacheListOriginIsListed,
                                    0);
}

}  // namespace safe_browsing
