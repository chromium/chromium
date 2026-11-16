// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"

#include "build/build_config.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace feed {
namespace internal {
namespace {
using history::DeletionInfo;

TEST(ShouldClearFeed, ShouldClearFeed) {
  EXPECT_TRUE(ShouldClearFeed(true, DeletionInfo::ForAllHistory()));
  EXPECT_FALSE(
      ShouldClearFeed(true, DeletionInfo::ForUrls(
                                {
                                    history::URLRow(GURL("http://url1")),
                                    history::URLRow(GURL("http://url2")),
                                },
                                /*favicon_urls=*/{})));
  EXPECT_FALSE(ShouldClearFeed(false, DeletionInfo::ForAllHistory()));
}

}  // namespace

}  // namespace internal

namespace {

class FeedServiceIsEnabledTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(prefs::kEnableSnippets,
                                                  /*default_value=*/true);
#if BUILDFLAG(IS_ANDROID)
    // Don't depend on the form factor of the device running the test.
    base::android::device_info::set_is_desktop_for_testing(
        /*is_desktop=*/false);
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_is_desktop_for_testing();
#endif
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(FeedServiceIsEnabledTest, EnabledByPref) {
  EXPECT_TRUE(FeedService::IsEnabled(pref_service_));

  pref_service_.SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_FALSE(FeedService::IsEnabled(pref_service_));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(FeedServiceIsEnabledTest, DisabledOnAndroidDesktop) {
  EXPECT_TRUE(FeedService::IsEnabled(pref_service_));

  base::android::device_info::set_is_desktop_for_testing(
      /*is_desktop=*/true);
  EXPECT_FALSE(FeedService::IsEnabled(pref_service_));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace feed
