// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_configuration.h"

#include "base/files/file_path.h"

namespace reporting {

namespace {

// Parameters of individual queues.
// TODO(b/159352842): Deliver space and upload parameters from outside.

constexpr base::FilePath::CharType kSecurityQueueSubdir[] =
    FILE_PATH_LITERAL("Security");
constexpr base::FilePath::CharType kSecurityQueuePrefix[] =
    FILE_PATH_LITERAL("P_Security");

constexpr base::FilePath::CharType kImmediateQueueSubdir[] =
    FILE_PATH_LITERAL("Immediate");
constexpr base::FilePath::CharType kImmediateQueuePrefix[] =
    FILE_PATH_LITERAL("P_Immediate");

constexpr base::FilePath::CharType kFastBatchQueueSubdir[] =
    FILE_PATH_LITERAL("FastBatch");
constexpr base::FilePath::CharType kFastBatchQueuePrefix[] =
    FILE_PATH_LITERAL("P_FastBatch");
constexpr base::TimeDelta kFastBatchUploadPeriod = base::Seconds(1);

constexpr base::FilePath::CharType kSlowBatchQueueSubdir[] =
    FILE_PATH_LITERAL("SlowBatch");
constexpr base::FilePath::CharType kSlowBatchQueuePrefix[] =
    FILE_PATH_LITERAL("P_SlowBatch");
constexpr base::TimeDelta kSlowBatchUploadPeriod = base::Seconds(20);

constexpr base::FilePath::CharType kBackgroundQueueSubdir[] =
    FILE_PATH_LITERAL("Background");
constexpr base::FilePath::CharType kBackgroundQueuePrefix[] =
    FILE_PATH_LITERAL("P_Background");
constexpr base::TimeDelta kBackgroundQueueUploadPeriod = base::Minutes(1);

constexpr base::FilePath::CharType kManualQueueSubdir[] =
    FILE_PATH_LITERAL("Manual");
constexpr base::FilePath::CharType kManualQueuePrefix[] =
    FILE_PATH_LITERAL("P_Manual");
constexpr base::TimeDelta kManualUploadPeriod = base::TimeDelta::Max();

constexpr base::FilePath::CharType kManualLacrosQueueSubdir[] =
    FILE_PATH_LITERAL("ManualLacros");
constexpr base::FilePath::CharType kManualLacrosQueuePrefix[] =
    FILE_PATH_LITERAL("P_ManualLacros");

// Failed upload retry delay: if an upload fails and there are no more incoming
// events, collected events will not get uploaded for an indefinite time (see
// b/192666219).
constexpr base::TimeDelta kFailedUploadRetryDelay = base::Seconds(1);

}  // namespace

StorageOptions::StorageOptions()
    : memory_resource_(base::MakeRefCounted<ResourceManager>(
          4u * 1024uLL * 1024uLL)),  // 4 MiB by default
      disk_space_resource_(base::MakeRefCounted<ResourceManager>(
          64u * 1024uLL * 1024uLL))  // 64 MiB by default.
{}
StorageOptions::StorageOptions(const StorageOptions& options) = default;
StorageOptions::~StorageOptions() = default;

// Returns vector of <priority, queue_options> for all expected queues in
// Storage. Queues are all located under the given root directory.
StorageOptions::QueuesOptionsList StorageOptions::ProduceQueuesOptions() const {
  return {
      std::make_pair(MANUAL_BATCH_LACROS,
                     QueueOptions(*this)
                         .set_subdirectory(kManualLacrosQueueSubdir)
                         .set_file_prefix(kManualLacrosQueuePrefix)
                         .set_upload_period(kManualUploadPeriod)
                         .set_upload_retry_delay(kFailedUploadRetryDelay)),
      std::make_pair(MANUAL_BATCH,
                     QueueOptions(*this)
                         .set_subdirectory(kManualQueueSubdir)
                         .set_file_prefix(kManualQueuePrefix)
                         .set_upload_period(kManualUploadPeriod)
                         .set_upload_retry_delay(kFailedUploadRetryDelay)),
      std::make_pair(BACKGROUND_BATCH,
                     QueueOptions(*this)
                         .set_subdirectory(kBackgroundQueueSubdir)
                         .set_file_prefix(kBackgroundQueuePrefix)
                         .set_upload_period(kBackgroundQueueUploadPeriod)),
      std::make_pair(SLOW_BATCH,
                     QueueOptions(*this)
                         .set_subdirectory(kSlowBatchQueueSubdir)
                         .set_file_prefix(kSlowBatchQueuePrefix)
                         .set_upload_period(kSlowBatchUploadPeriod)),
      std::make_pair(FAST_BATCH,
                     QueueOptions(*this)
                         .set_subdirectory(kFastBatchQueueSubdir)
                         .set_file_prefix(kFastBatchQueuePrefix)
                         .set_upload_period(kFastBatchUploadPeriod)),
      std::make_pair(IMMEDIATE,
                     QueueOptions(*this)
                         .set_subdirectory(kImmediateQueueSubdir)
                         .set_file_prefix(kImmediateQueuePrefix)
                         .set_upload_retry_delay(kFailedUploadRetryDelay)),
      std::make_pair(SECURITY,
                     QueueOptions(*this)
                         .set_subdirectory(kSecurityQueueSubdir)
                         .set_file_prefix(kSecurityQueuePrefix)
                         .set_upload_retry_delay(kFailedUploadRetryDelay)
                         .set_can_shed_records(false)),
  };
}

QueueOptions::QueueOptions(const StorageOptions& storage_options)
    : storage_options_(storage_options) {}
QueueOptions::QueueOptions(const QueueOptions& options) = default;
}  // namespace reporting
