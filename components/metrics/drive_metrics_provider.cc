// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/files/drive_info.h"
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

namespace {
void RecordTriStateMetric(const char* name, std::optional<bool> sample) {
  base::UmaHistogramEnumeration(
      name, !sample.has_value()
                ? DriveMetricsProvider::OptionalBoolRecord::kUnknown
                : (*sample ? DriveMetricsProvider::OptionalBoolRecord::kTrue
                           : DriveMetricsProvider::OptionalBoolRecord::kFalse));
}
}  // namespace

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

DriveMetricsProvider::SeekPenaltyResponse::SeekPenaltyResponse() = default;

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

  bool has_seek_penalty;
  bool have_value = HasSeekPenalty(path, &has_seek_penalty);
  if (have_value) {
    response->has_seek_penalty = has_seek_penalty;
  }
  std::optional<base::DriveInfo> drive_info = base::GetFileDriveInfo(path);
  if (drive_info.has_value()) {
    response->has_seek_penalty_base = drive_info->has_seek_penalty;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    response->is_removable = drive_info->is_removable;
    response->is_usb = drive_info->is_usb;
#endif
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
  if (response.has_seek_penalty.has_value()) {
    drive->set_has_seek_penalty(*response.has_seek_penalty);
  }

  RecordTriStateMetric("UMA.SeekPenaltyResult.Provider",
                       response.has_seek_penalty);
  RecordTriStateMetric("UMA.SeekPenaltyResult.Base",
                       response.has_seek_penalty_base);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  RecordTriStateMetric("UMA.DriveIsRemovableResult", response.is_removable);
  RecordTriStateMetric("UMA.DriveIsUSBResult", response.is_usb);
#endif
}

}  // namespace metrics
