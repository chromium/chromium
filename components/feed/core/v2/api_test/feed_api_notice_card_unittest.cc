// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {
namespace {
class FeedApiNoticeCardTest : public FeedApiTest {
 public:
  void SetUp() override {
    FeedApiTest::SetUp();
    model_generator_.privacy_notice_fulfilled = true;
  }

 protected:
  StreamModelUpdateRequestGenerator model_generator_;
};

TEST_F(FeedApiNoticeCardTest, LoadStreamSendsNoticeCardAcknowledgement) {
  response_translator_.InjectResponse(model_generator_.MakeFirstPage());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Generate 3 view actions and 1 click action to trigger the acknowledgement
  // of the notice card.
  const int notice_card_index = 0;
  std::string slice_id =
      surface.initial_state->updated_slices(notice_card_index)
          .slice()
          .slice_id();
  stream_->ReportSliceViewed(surface.GetSurfaceId(), slice_id);
  task_environment_.FastForwardBy(base::Hours(1));
  stream_->ReportSliceViewed(surface.GetSurfaceId(), slice_id);
  task_environment_.FastForwardBy(base::Hours(1));
  stream_->ReportSliceViewed(surface.GetSurfaceId(), slice_id);
  stream_->ReportOpenAction(GURL(), surface.GetSurfaceId(), slice_id,
                            OpenActionType::kDefault);

  response_translator_.InjectResponse(model_generator_.MakeFirstPage());
  stream_->UnloadModel(surface.GetStreamType());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .feed_query()
                  .chrome_fulfillment_info()
                  .notice_card_acknowledged());
}

TEST_F(FeedApiNoticeCardTest, LoadStreamUpdateNoticeCardFulfillmentHistogram) {
  base::HistogramTester histograms;

  // Trigger a stream refresh that updates the histogram.
  {
    auto model_state = model_generator_.MakeFirstPage();
    model_state->stream_data.set_privacy_notice_fulfilled(false);
    response_translator_.InjectResponse(std::move(model_state));

    refresh_scheduler_.Clear();
    stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
    WaitForIdleTaskQueue();
  }

  UnloadModel(StreamType(StreamKind::kForYou));

  // Trigger another stream refresh that updates the histogram.
  {
    auto model_state = model_generator_.MakeFirstPage();
    model_state->stream_data.set_privacy_notice_fulfilled(true);
    response_translator_.InjectResponse(std::move(model_state));

    refresh_scheduler_.Clear();
    stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
    WaitForIdleTaskQueue();
  }

  // Verify that the notice card fulfillment histogram was properly recorded.
  histograms.ExpectBucketCount("ContentSuggestions.Feed.NoticeCardFulfilled2",
                               0, 1);
  histograms.ExpectBucketCount("ContentSuggestions.Feed.NoticeCardFulfilled2",
                               1, 1);
}

}  // namespace
}  // namespace test
}  // namespace feed
