// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_score_provider.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

namespace {

const SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

class SegmentScoreProviderTest : public testing::Test {
 public:
  SegmentScoreProviderTest() = default;
  ~SegmentScoreProviderTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    single_segment_manager_ =
        SegmentScoreProvider::Create(segment_database_.get(), {kSegmentId});
  }

  void InitializeMetadataForSegment(SegmentId segment_id,
                                    float mapping[][2],
                                    int num_mapping_pairs) {
    auto* metadata = segment_database_->FindOrCreateSegment(segment_id)
                         ->mutable_model_metadata();
    metadata->set_result_time_to_live(7);
    segment_database_->SetBucketDuration(segment_id, 1, proto::TimeUnit::DAY);

    std::string default_mapping_key = "some_key";
    metadata->set_default_discrete_mapping(default_mapping_key);
    segment_database_->AddDiscreteMapping(
        segment_id, mapping, num_mapping_pairs, default_mapping_key);
  }

  void GetSegmentScore(SegmentId segment_id, const SegmentScore& expected) {
    base::RunLoop loop;
    single_segment_manager_->GetSegmentScore(
        segment_id,
        base::BindOnce(&SegmentScoreProviderTest::OnGetSegmentScore,
                       base::Unretained(this), loop.QuitClosure(), expected));
    loop.Run();
  }

  void OnGetSegmentScore(base::RepeatingClosure closure,
                         const SegmentScore& expected,
                         const SegmentScore& actual) {
    ASSERT_EQ(expected.scores, actual.scores);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<SegmentScoreProvider> single_segment_manager_;
};

TEST_F(SegmentScoreProviderTest, GetSegmentScore) {
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(kSegmentId, mapping1, 3);
  segment_database_->AddPredictionResult(kSegmentId, 0.6, base::Time::Now());

  base::RunLoop loop;
  single_segment_manager_->Initialize(loop.QuitClosure());
  loop.Run();

  // Returns results from last session.
  SegmentScore expected;
  expected.scores = {0.6};
  GetSegmentScore(kSegmentId, expected);

  // Updating the scores in the current session doesn't affect the get call.
  segment_database_->AddPredictionResult(kSegmentId, 0.8, base::Time::Now());
  GetSegmentScore(kSegmentId, expected);

  // Returns empty results when called on a segment with no scores.
  GetSegmentScore(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE,
                  SegmentScore());
}

}  // namespace
}  // namespace segmentation_platform
