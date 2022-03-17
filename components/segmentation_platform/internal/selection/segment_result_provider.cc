// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include <map>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

namespace segmentation_platform {
namespace {

class SegmentResultProviderImpl : public SegmentResultProvider {
 public:
  SegmentResultProviderImpl(SegmentInfoDatabase* segment_database,
                            SignalStorageConfig* signal_storage_config,
                            ModelProviderFactory* model_provider_factory,
                            base::Clock* clock,
                            bool force_refresh_results)
      : segment_database_(segment_database),
        signal_storage_config_(signal_storage_config),
        model_provider_factory_(model_provider_factory),
        clock_(clock),
        force_refresh_results_(force_refresh_results),
        task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  void GetSegmentResult(OptimizationTarget segment_id,
                        const std::string& segmentation_key,
                        SegmentResultCallback callback) override;

  SegmentResultProviderImpl(SegmentResultProviderImpl&) = delete;
  SegmentResultProviderImpl& operator=(SegmentResultProviderImpl&) = delete;

 private:
  void OnGetSegmentInfo(OptimizationTarget segment_id,
                        const std::string& segmentation_key,
                        SegmentResultCallback callback,
                        absl::optional<proto::SegmentInfo> available_segment);

  const raw_ptr<SegmentInfoDatabase> segment_database_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<ModelProviderFactory> model_provider_factory_;
  const raw_ptr<base::Clock> clock_;
  const bool force_refresh_results_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SegmentResultProviderImpl> weak_ptr_factory_{this};
};

void SegmentResultProviderImpl::GetSegmentResult(
    OptimizationTarget segment_id,
    const std::string& segmentation_key,
    SegmentResultCallback callback) {
  segment_database_->GetSegmentInfo(
      segment_id, base::BindOnce(&SegmentResultProviderImpl::OnGetSegmentInfo,
                                 weak_ptr_factory_.GetWeakPtr(), segment_id,
                                 segmentation_key, std::move(callback)));
}

void SegmentResultProviderImpl::OnGetSegmentInfo(
    OptimizationTarget segment_id,
    const std::string& segmentation_key,
    SegmentResultCallback callback,
    absl::optional<proto::SegmentInfo> available_segment) {
  // Don't compute results if we don't have enough signals, or don't have
  // valid unexpired results for any of the segments.
  proto::SegmentInfo* segment_info = nullptr;
  if (available_segment) {
    segment_info = &available_segment.value();
  } else {
    VLOG(1) << __func__ << ": segment=" << OptimizationTarget_Name(segment_id)
            << " does not have segment info.";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::make_unique<SegmentResult>(
                                      ResultState::kSegmentNotAvailable)));
    return;
  }

  // TODO(ssid): Remove this check since scheduler does this before executing
  // the model.
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info->model_metadata())) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(segment_info->segment_id())
            << " does not meet signal collection requirements.";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::make_unique<SegmentResult>(
                                      ResultState::kSignalsNotCollected)));
    return;
  }

  if (metadata_utils::HasExpiredOrUnavailableResult(*segment_info,
                                                    clock_->Now())) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(segment_info->segment_id())
            << " has expired or unavailable result.";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::make_unique<SegmentResult>(
                                      ResultState::kDatabaseScoreNotReady)));
    return;
  }

  int rank = metadata_utils::ConvertToDiscreteScore(
      segmentation_key, segment_info->prediction_result().result(),
      segment_info->model_metadata());
  VLOG(1) << __func__ << ": segment=" << OptimizationTarget_Name(segment_id)
          << ": result=" << segment_info->prediction_result().result()
          << ", rank=" << rank;

  auto result =
      std::make_unique<SegmentResult>(ResultState::kSuccessFromDatabase, rank);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace

SegmentResultProvider::SegmentResult::SegmentResult(ResultState state)
    : state(state) {}
SegmentResultProvider::SegmentResult::SegmentResult(ResultState state, int rank)
    : state(state), rank(rank) {}
SegmentResultProvider::SegmentResult::~SegmentResult() = default;

// static
std::unique_ptr<SegmentResultProvider> SegmentResultProvider::Create(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    ModelProviderFactory* model_provider_factory,
    base::Clock* clock,
    bool force_refresh_results) {
  return std::make_unique<SegmentResultProviderImpl>(
      segment_database, signal_storage_config, model_provider_factory, clock,
      force_refresh_results);
}

}  // namespace segmentation_platform
