// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include "base/callback_list.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using testing::Test;

constexpr char kSessionTag1[] = "foreign1";

class SessionSyncPrefsTest : public Test {
 protected:
  SessionSyncPrefsTest()
      : session_sync_prefs_(RegisterAndReturnPrefs(&pref_service_)) {}

  static PrefService* RegisterAndReturnPrefs(
      TestingPrefServiceSimple* pref_service) {
    SessionSyncPrefs::RegisterProfilePrefs(pref_service->registry());
    return pref_service;
  }

  TestingPrefServiceSimple pref_service_;
  SessionSyncPrefs session_sync_prefs_;
};

// Verifies that `SetSessionVisibility()` updates the session's visibility in
// `SessionSyncPrefs` and notifies subscribers only when the visibility state
// changes.
TEST_F(SessionSyncPrefsTest, ShouldSetSessionVisibilityAndNotifySubscribers) {
  base::MockRepeatingClosure visibility_changed_cb;
  base::CallbackListSubscription subscription =
      session_sync_prefs_.SubscribeToSessionVisibilityChanged(
          visibility_changed_cb.Get());

  EXPECT_EQ(session_sync_prefs_.GetSessionVisibility(kSessionTag1),
            SessionVisibility::kVisible);

  // Hiding a visible session updates the preference and notifies subscribers.
  EXPECT_CALL(visibility_changed_cb, Run()).Times(1);
  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kHidden);
  EXPECT_EQ(session_sync_prefs_.GetSessionVisibility(kSessionTag1),
            SessionVisibility::kHidden);
  testing::Mock::VerifyAndClearExpectations(&visibility_changed_cb);

  // Setting the same visibility state is a no-op and does not fire a redundant
  // notification.
  EXPECT_CALL(visibility_changed_cb, Run()).Times(0);
  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kHidden);
  testing::Mock::VerifyAndClearExpectations(&visibility_changed_cb);

  // Unhiding a hidden session restores default visibility and notifies
  // subscribers.
  EXPECT_CALL(visibility_changed_cb, Run()).Times(1);
  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kVisible);
  EXPECT_EQ(session_sync_prefs_.GetSessionVisibility(kSessionTag1),
            SessionVisibility::kVisible);
}

// Verifies that `GarbageCollectUnknownHiddenSessions()` prunes hidden session
// tags that are not in the tracked set while preserving tracked hidden
// sessions, without firing `SubscribeToSessionVisibilityChanged()`
// notifications.
TEST_F(SessionSyncPrefsTest,
       GarbageCollectUnknownHiddenSessionsShouldNotNotifySubscribers) {
  constexpr char kSessionTag2[] = "foreign2";
  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kHidden);
  session_sync_prefs_.SetSessionVisibility(kSessionTag2,
                                           SessionVisibility::kHidden);

  base::MockRepeatingClosure visibility_changed_cb;
  base::CallbackListSubscription subscription =
      session_sync_prefs_.SubscribeToSessionVisibilityChanged(
          visibility_changed_cb.Get());

  EXPECT_CALL(visibility_changed_cb, Run()).Times(0);
  session_sync_prefs_.GarbageCollectUnknownHiddenSessions({kSessionTag1});

  EXPECT_EQ(session_sync_prefs_.GetSessionVisibility(kSessionTag1),
            SessionVisibility::kHidden);
  EXPECT_EQ(session_sync_prefs_.GetSessionVisibility(kSessionTag2),
            SessionVisibility::kVisible);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verifies that `IsDeviceTabVisibilityEnabled()` reflects the
// `kDeviceTabVisibilitySettings` feature flag on desktop platforms.
TEST_F(SessionSyncPrefsTest, IsDeviceTabVisibilityEnabledReflectsFeatureFlag) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        syncer::kDeviceTabVisibilitySettings);
    EXPECT_FALSE(SessionSyncPrefs::IsDeviceTabVisibilityEnabled());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list(
        syncer::kDeviceTabVisibilitySettings);
    EXPECT_TRUE(SessionSyncPrefs::IsDeviceTabVisibilityEnabled());
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace sync_sessions
