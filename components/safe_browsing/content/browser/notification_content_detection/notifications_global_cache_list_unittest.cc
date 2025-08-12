// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"

#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

using NotificationsGlobalCacheListTest = testing::Test;

TEST_F(NotificationsGlobalCacheListTest, IsDomainInList) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(
      IsDomainInNotificationsGlobalCacheList(GURL("https://google.com")));
#else
  EXPECT_FALSE(
      IsDomainInNotificationsGlobalCacheList(GURL("https://google.com")));
#endif
}

TEST_F(NotificationsGlobalCacheListTest, InvalidURL) {
  EXPECT_FALSE(IsDomainInNotificationsGlobalCacheList(GURL("not-a-url")));
  EXPECT_FALSE(IsDomainInNotificationsGlobalCacheList(GURL("")));
}

TEST_F(NotificationsGlobalCacheListTest, GetAndSetDomains) {
  std::vector<std::string> domains = {"one.com", "two.com", "three.com"};
  SetNotificationsGlobalCacheListDomainsForTesting(domains);
  EXPECT_EQ(GetNotificationsGlobalCacheListDomains(), domains);

  SetNotificationsGlobalCacheListDomainsForTesting({});
  EXPECT_TRUE(GetNotificationsGlobalCacheListDomains().empty());
}

TEST_F(NotificationsGlobalCacheListTest, IsSubDomainInList) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(IsDomainInNotificationsGlobalCacheList(
      GURL("https://google.com/foo/bar.html")));
#else
  EXPECT_FALSE(IsDomainInNotificationsGlobalCacheList(
      GURL("https://google.com/foo/bar.html")));
#endif
}

}  // namespace safe_browsing
