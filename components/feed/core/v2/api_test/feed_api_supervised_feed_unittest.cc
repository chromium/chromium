// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/proto/v2/wire/feed_entry_point_source.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is for testing the Supervised Feed content.

namespace feed::test {
namespace {

class FeedApiSupervisedUserTest : public FeedApiTest {
 public:
  void SetUp() override {
    supervised_user::RegisterProfilePrefs(profile_prefs_.registry());
    supervised_user::EnableParentalControls(profile_prefs_);
    is_supervised_account_ = true;

    feature_list_.InitAndEnableFeature(
        supervised_user::kKidFriendlyContentFeed);
    FeedApiTest::SetUp();
  }

 protected:
  // Returns model state for `StreamKind::kSupervisedUser`.
  std::unique_ptr<StreamModelUpdateRequest>
  MakeTypicalInitialModelStateForSupervisedUser() {
    return MakeTypicalInitialModelState(
        0, kTestTimeEpoch, /*signed_in=*/true, /*logging_enabled=*/true,
        /*privacy_notice_fulfilled=*/false,
        feedstore::StreamKey(StreamType(StreamKind::kSupervisedUser)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Shows feed content for supervised users.
TEST_F(FeedApiSupervisedUserTest, LoadSupervisedFeed) {
  response_translator_.InjectResponse(
      MakeTypicalInitialModelStateForSupervisedUser());
  TestSupervisedFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_TRUE(network_.query_request_sent);
  EXPECT_EQ(
      std::vector<NetworkRequestType>({NetworkRequestType::kSupervisedFeed}),
      network_.sent_request_types());

  StreamType stream_type(StreamKind::kSupervisedUser);
  // Verify the model is filled correctly.
  ASSERT_EQ(stream_->GetModel(stream_type)->DumpStateForTesting(),
            ModelStateFor(stream_type, store_.get()));
  // Verify the data is written to the store.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelStateForSupervisedUser()),
      ModelStateFor(stream_type, store_.get()));
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

// Supervised feed is deleted after surface is detached.
TEST_F(FeedApiSupervisedUserTest, DeleteSupervisedFeedOnDetachedSurface) {
  response_translator_.InjectResponse(
      MakeTypicalInitialModelStateForSupervisedUser());
  TestSupervisedFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  WaitForModelToAutoUnload();
  WaitForIdleTaskQueue();

  StreamType stream_type(StreamKind::kSupervisedUser);
  EXPECT_FALSE(stream_->GetModel(stream_type));
}

// Supervised feed should ignore quota.
TEST_F(FeedApiSupervisedUserTest, SupervisedFeedShouldIgnoreQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (int i = 0; i < 50; i++) {
    status =
        stream_
            ->ShouldMakeFeedQueryRequest(
                StreamType(StreamKind::kSupervisedUser), LoadType::kInitialLoad)
            .load_stream_status;
  }

  ASSERT_EQ(LoadStreamStatus::kNoStatus, status);
}

TEST_F(FeedApiSupervisedUserTest, WebFeedsDisabledForSupervisedAccounts) {
  SetFeedConfigForTesting(Config());
  response_translator_.InjectResponse(
      MakeTypicalInitialModelStateForSupervisedUser());
  TestSupervisedFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_FALSE(stream_->IsWebFeedEnabled());

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(0, network_.GetListFollowedWebFeedsRequestCount());
}

}  // namespace
}  // namespace feed::test
