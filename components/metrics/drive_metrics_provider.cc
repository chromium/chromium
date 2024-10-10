// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/files/drive_info.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace metrics {

DriveMetricsProvider::DriveMetricsProvider(int local_state_path_key)
    : local_state_path_key_(local_state_path_key) {}

DriveMetricsProvider::~DriveMetricsProvider() = default;

void DriveMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  auto* hardware = system_profile_proto->mutable_hardware();
  FillDriveMetrics(metrics_.app_drive, hardware->mutable_app_drive());
  FillDriveMetrics(metrics_.user_data_drive,
                   hardware->mutable_user_data_drive());
}

void DriveMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DriveMetricsProvider::GetDriveMetricsOnBackgroundThread,
                     local_state_path_key_),
      base::BindOnce(&DriveMetricsProvider::GotDriveMetrics,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)));
}

DriveMetricsProvider::SeekPenaltyResponse::SeekPenaltyResponse()
    : success(false) {}

// static
DriveMetricsProvider::DriveMetrics
DriveMetricsProvider::GetDriveMetricsOnBackgroundThread(
    int local_state_path_key) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  DriveMetricsProvider::DriveMetrics metrics;
  QuerySeekPenalty(base::FILE_EXE, &metrics.app_drive);
  QuerySeekPenalty(local_state_path_key, &metrics.user_data_drive);
  return metrics;
}

// static
void DriveMetricsProvider::QuerySeekPenalty(
    int path_service_key,
    DriveMetricsProvider::SeekPenaltyResponse* response) {
  DCHECK(response);

  base::FilePath path;
  if (!base::PathService::Get(path_service_key, &path))
    return;

  response->success = HasSeekPenalty(path, &response->has_seek_penalty);
  std::optional<base::DriveInfo> drive_info = base::GetFileDriveInfo(path);
  response->success_base =
      drive_info.has_value() && drive_info->has_seek_penalty.has_value();
  if (response->success_base) {
    response->has_seek_penalty_base = *drive_info->has_seek_penalty;
  }
}

void DriveMetricsProvider::GotDriveMetrics(
    base::OnceClosure done_callback,
    const DriveMetricsProvider::DriveMetrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_ = metrics;
  std::move(done_callback).Run();
}

void DriveMetricsProvider::FillDriveMetrics(
    const DriveMetricsProvider::SeekPenaltyResponse& response,
    metrics::SystemProfileProto::Hardware::Drive* drive) {
  if (response.success) {
    drive->set_has_seek_penalty(response.has_seek_penalty);
  }

  base::UmaHistogramEnumeration(
      "UMA.SeekPenaltyResult.Provider",
      !response.success
          ? SeekPenaltyRecord::kUnknown
          : (response.has_seek_penalty ? SeekPenaltyRecord::kTrue
                                       : SeekPenaltyRecord::kFalse));
  base::UmaHistogramEnumeration(
      "UMA.SeekPenaltyResult.Base",
      !response.success_base
          ? SeekPenaltyRecord::kUnknown
          : (response.has_seek_penalty_base ? SeekPenaltyRecord::kTrue
                                            : SeekPenaltyRecord::kFalse));
}

}  // namespace metrics
