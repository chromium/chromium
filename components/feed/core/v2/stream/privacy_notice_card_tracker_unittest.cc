// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/privacy_notice_card_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

feedwire::ContentId NoticeCardContentId() {
  feedwire::ContentId id;
  id.set_content_domain("privacynoticecard.f");
  return id;
}

feedwire::ContentId OtherContentId() {
  feedwire::ContentId id;
  id.set_content_domain("NOTprivacynoticecard.f");
  return id;
}

class PrivacyNoticeCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    // Make sure current time isn't zero.
    task_environment_.AdvanceClock(base::Days(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
};

TEST_F(PrivacyNoticeCardTrackerTest,
       TrackingNoticeCardActionsDoesntUpdateCountsForNonNoticeCard) {
  PrivacyNoticeCardTracker tracker(&profile_prefs_);

  // Generate enough views to reach the acknowlegement threshold, but the views
  // were not on the notice card.
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());
  task_environment_.AdvanceClock(base::Minutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());
  task_environment_.AdvanceClock(base::Minutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(PrivacyNoticeCardTrackerTest, AcknowledgedNoticeCardWhenEnoughViews) {
  PrivacyNoticeCardTracker tracker(&profile_prefs_);

  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::Minutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::Minutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_TRUE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(PrivacyNoticeCardTrackerTest, ViewsAreIgnoredIfNotEnoughTimeElapsed) {
  PrivacyNoticeCardTracker tracker(&profile_prefs_);

  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::Minutes(4));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::Minutes(4));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(PrivacyNoticeCardTrackerTest,
       DontAcknowledgedNoticeCardWhenNotEnoughViewsNorClicks) {
  PrivacyNoticeCardTracker tracker(&profile_prefs_);

  // Generate views but not enough to reach the threshold.
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::Minutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

}  // namespace
}  // namespace feed
