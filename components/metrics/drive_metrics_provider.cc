// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace metrics {

DriveMetricsProvider::DriveMetricsProvider(int local_state_path_key)
    : local_state_path_key_(local_state_path_key) {}

DriveMetricsProvider::~DriveMetricsProvider() {}

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
  if (response.success)
    drive->set_has_seek_penalty(response.has_seek_penalty);
}

}  // namespace metrics
