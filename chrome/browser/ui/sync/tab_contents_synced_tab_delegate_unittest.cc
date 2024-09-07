// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/test_synced_window_delegates_getter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

// The minimum time between two sync updates of `last_active_time` when the tab
// hasn't changed.
constexpr base::TimeDelta kSyncActiveTimeThreshold = base::Minutes(10);

class TestSyncedTabDelegate : public TabContentsSyncedTabDelegate {
 public:
  explicit TestSyncedTabDelegate(content::WebContents* web_contents) {
    SetWebContents(web_contents);
  }

  ~TestSyncedTabDelegate() override = default;

  SessionID GetWindowId() const override { return SessionID::InvalidValue(); }
  SessionID GetSessionId() const override { return SessionID::InvalidValue(); }
  bool IsPlaceholderTab() const override { return false; }
  bool IsInitialBlankNavigation() const override {
    // Returns false so the ShouldSyncReturnsFalse* tests would not return early
    // because this function returns true.
    return false;
  }
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      sync_sessions::SyncSessionsClient* sessions_client) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

class TabContentsSyncedTabDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TabContentsSyncedTabDelegateTest() {
    ON_CALL(mock_sync_sessions_client_, GetSyncedWindowDelegatesGetter())
        .WillByDefault(testing::Return(&window_getter_));
  }
  ~TabContentsSyncedTabDelegateTest() override = default;

 protected:
  sync_sessions::TestSyncedWindowDelegatesGetter window_getter_;
  testing::NiceMock<sync_sessions::MockSyncSessionsClient>
      mock_sync_sessions_client_;
};

TEST_F(TabContentsSyncedTabDelegateTest, InvalidEntryIndexReturnsDefault) {
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  TestSyncedTabDelegate delegate(web_contents.get());
  NavigateAndCommit(GURL("about:blank"));

  sessions::SerializedNavigationEntry serialized_entry;

  // -1 and 2 are invalid indices because there's only one navigation
  // recorded(the initial one to "about:blank")
  delegate.GetSerializedNavigationAtIndex(-1, &serialized_entry);
  EXPECT_EQ(serialized_entry.virtual_url(), GURL());

  delegate.GetSerializedNavigationAtIndex(2, &serialized_entry);
  EXPECT_EQ(serialized_entry.virtual_url(), GURL());
}

// Test that ShouldSync will return false if the WebContents has not navigated
// anywhere yet. The WebContents will be on the initial NavigationEntry and
// have nothing to sync, because the function will return "false" early (rather
// than iterate through the entries list).
TEST_F(TabContentsSyncedTabDelegateTest,
       ShouldSyncReturnsFalseOnWebContentsOnInitialNavigationEntry) {
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  TestSyncedTabDelegate delegate(web_contents.get());
  window_getter_.AddWindow(sync_pb::SyncEnums_BrowserType_TYPE_TABBED,
                           delegate.GetWindowId());

  // The WebContents has not navigated, so it's on the initial NavigationEntry.
  ASSERT_TRUE(
      web_contents->GetController().GetLastCommittedEntry()->IsInitialEntry());

  // TestSyncedTabDelegate intentionally returns false for
  // IsInitialBlankNavigation() even though no navigation has committed
  // to ensure ShouldSync() won't return early because of it (which is possible
  // in case the tab was restored before and hasn't navigated anywhere).
  ASSERT_FALSE(delegate.IsInitialBlankNavigation());

  // ShouldSync should return false because there it's on the initial
  // NavigationEntry.
  EXPECT_FALSE(delegate.ShouldSync(&mock_sync_sessions_client_));
}

// Tests that GetLastActiveTime() is returning the cached value if less time
// than a threshold has passed, and is returning the WebContents last active
// time if more time has passed.
TEST_F(TabContentsSyncedTabDelegateTest, CachedLastActiveTime) {
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  TestSyncedTabDelegate delegate(web_contents.get());
  window_getter_.AddWindow(sync_pb::SyncEnums_BrowserType_TYPE_TABBED,
                           delegate.GetWindowId());

  base::TimeTicks original_time_ticks = base::TimeTicks::Now();
  base::Time original_time = base::Time() + base::Seconds(1);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(original_time);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTimeTicks(original_time_ticks);

  EXPECT_EQ(original_time, delegate.GetLastActiveTime());

  // If not enough time has passed, the cached time should be returned.
  base::TimeTicks before_threshold_ticks =
      original_time_ticks + kSyncActiveTimeThreshold - base::Minutes(1);
  base::Time before_threshold = base::Time() + base::Seconds(2);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTimeTicks(before_threshold_ticks);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(before_threshold);
  EXPECT_EQ(original_time, delegate.GetLastActiveTime());

  // After the threshold has passed, the new value should be returned.
  base::TimeTicks after_threshold_ticks =
      original_time_ticks + kSyncActiveTimeThreshold + base::Minutes(1);
  base::Time after_threshold = base::Time() + base::Seconds(3);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTimeTicks(after_threshold_ticks);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(after_threshold);
  EXPECT_EQ(after_threshold, delegate.GetLastActiveTime());
}

// Tests that the resetting the cached value of last_active_time allows to
// return the value from the WebState even if less time than the threshold has
// passed.
TEST_F(TabContentsSyncedTabDelegateTest, ResetCachedLastActiveTime) {
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  TestSyncedTabDelegate delegate(web_contents.get());
  window_getter_.AddWindow(sync_pb::SyncEnums_BrowserType_TYPE_TABBED,
                           delegate.GetWindowId());

  base::TimeTicks original_time_ticks = base::TimeTicks::Now();
  base::Time original_time = base::Time() + base::Seconds(1);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTimeTicks(original_time_ticks);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(original_time);

  EXPECT_EQ(original_time, delegate.GetLastActiveTime());

  delegate.ResetCachedLastActiveTime();

  // Even if not enough time has passed, the cached time should not be returned.
  base::TimeTicks before_threshold_ticks =
      original_time_ticks + kSyncActiveTimeThreshold - base::Minutes(1);
  base::Time before_threshold = base::Time() + base::Seconds(2);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTimeTicks(before_threshold_ticks);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(before_threshold);
  EXPECT_EQ(before_threshold, delegate.GetLastActiveTime());
}

}  // namespace
