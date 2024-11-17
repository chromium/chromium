// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_score_provider.h"

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
namespace {

class SegmentScoreProviderImpl : public SegmentScoreProvider {
 public:
  SegmentScoreProviderImpl(SegmentInfoDatabase* segment_database,
                           base::flat_set<proto::SegmentId> segment_ids)
      : segment_database_(segment_database),
        segment_ids_(std::move(segment_ids)) {}

  ~SegmentScoreProviderImpl() override = default;

  void Initialize(base::OnceClosure callback) override {
    // Read model results from DB.
    segment_database_->GetSegmentInfoForSegments(
        segment_ids_,
        base::BindOnce(&SegmentScoreProviderImpl::ReadScoresFromLastSession,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSegmentScore(SegmentId segment_id,
                       SegmentScoreCallback callback) override {
    DCHECK(initialized_);

    SegmentScore result;
    auto iter = scores_last_session_.find(segment_id);
    if (iter != scores_last_session_.end())
      result.scores = iter->second;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

 private:
  void ReadScoresFromLastSession(
      base::OnceClosure callback,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> all_segments) {
    // Read results from last session to memory.
    for (const auto& pair : *all_segments) {
      SegmentId id = pair.first;
      const proto::SegmentInfo& info = *pair.second;
      if (!info.has_prediction_result())
        continue;

      const auto& scores = info.prediction_result().result();
      scores_last_session_.emplace(
          id, ModelProvider::Response(scores.begin(), scores.end()));
    }

    initialized_ = true;
    std::move(callback).Run();
  }

  // The database retrieving results.
  raw_ptr<SegmentInfoDatabase> segment_database_;

  // List of all segment_ids to be fetched
  const base::flat_set<proto::SegmentId> segment_ids_;

  // Model scores that are read from db on startup and used for serving the
  // clients in the current session.
  std::map<SegmentId, ModelProvider::Response> scores_last_session_;

  // Whether the initialization is complete through an Initialize call.
  bool initialized_{false};

  base::WeakPtrFactory<SegmentScoreProviderImpl> weak_ptr_factory_{this};
};

}  // namespace

SegmentScore::SegmentScore() = default;

SegmentScore::SegmentScore(const SegmentScore& other) = default;

SegmentScore::~SegmentScore() = default;

std::unique_ptr<SegmentScoreProvider> SegmentScoreProvider::Create(
    SegmentInfoDatabase* segment_database,
    base::flat_set<proto::SegmentId> segment_ids) {
  return std::make_unique<SegmentScoreProviderImpl>(segment_database,
                                                    std::move(segment_ids));
}

}  // namespace segmentation_platform
