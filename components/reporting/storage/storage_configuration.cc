// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_configuration.h"

#include "base/files/file_path.h"
#include "base/logging.h"

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

// Order of priorities
constexpr std::array<Priority, 7> kPriorityOrder = {
    MANUAL_BATCH_LACROS, MANUAL_BATCH, BACKGROUND_BATCH, SLOW_BATCH,
    FAST_BATCH,          IMMEDIATE,    SECURITY};

// Failed upload retry delay: if an upload fails and there are no more incoming
// events, collected events will not get uploaded for an indefinite time (see
// b/192666219).
constexpr base::TimeDelta kFailedUploadRetryDelay = base::Seconds(1);

}  // namespace

StorageOptions::MultiGenerational::MultiGenerational() {
  for (const auto& priority : StorageOptions::GetPrioritiesOrder()) {
    is_multi_generational_[priority].store(false);
  }
}

bool StorageOptions::MultiGenerational::get(Priority priority) const {
  CHECK_LT(priority, Priority_ARRAYSIZE);
  return is_multi_generational_[priority].load();
}

void StorageOptions::MultiGenerational::set(Priority priority, bool state) {
  CHECK_LT(priority, Priority_ARRAYSIZE);
  const bool was_multigenerational =
      is_multi_generational_[priority].exchange(state);
  LOG_IF(WARNING, was_multigenerational != state)
      << "Priority " << Priority_Name(priority) << " switched to "
      << (state ? "multi" : "single") << "-generational state";
}

StorageOptions::StorageOptions(
    base::RepeatingCallback<void(Priority, QueueOptions&)>
        modify_queue_options_for_tests)
    : key_check_period_(kDefaultKeyCheckPeriod),  // 1 second by default
      is_multi_generational_(base::MakeRefCounted<MultiGenerational>()),
      memory_resource_(base::MakeRefCounted<ResourceManager>(
          4u * 1024uLL * 1024uLL)),  // 4 MiB by default
      disk_space_resource_(base::MakeRefCounted<ResourceManager>(
          64u * 1024uLL * 1024uLL)),  // 64 MiB by default.
      modify_queue_options_for_tests_(modify_queue_options_for_tests) {}

StorageOptions::StorageOptions(const StorageOptions& other) = default;
StorageOptions::~StorageOptions() = default;

QueueOptions StorageOptions::PopulateQueueOptions(Priority priority) const {
  switch (priority) {
    case MANUAL_BATCH_LACROS:
      return QueueOptions(*this)
          .set_subdirectory(kManualLacrosQueueSubdir)
          .set_file_prefix(kManualLacrosQueuePrefix)
          .set_upload_period(kManualUploadPeriod)
          .set_upload_retry_delay(kFailedUploadRetryDelay);
    case MANUAL_BATCH:
      return QueueOptions(*this)
          .set_subdirectory(kManualQueueSubdir)
          .set_file_prefix(kManualQueuePrefix)
          .set_upload_period(kManualUploadPeriod)
          .set_upload_retry_delay(kFailedUploadRetryDelay);
    case BACKGROUND_BATCH:
      return QueueOptions(*this)
          .set_subdirectory(kBackgroundQueueSubdir)
          .set_file_prefix(kBackgroundQueuePrefix)
          .set_upload_period(kBackgroundQueueUploadPeriod);
    case SLOW_BATCH:
      return QueueOptions(*this)
          .set_subdirectory(kSlowBatchQueueSubdir)
          .set_file_prefix(kSlowBatchQueuePrefix)
          .set_upload_period(kSlowBatchUploadPeriod);
    case FAST_BATCH:
      return QueueOptions(*this)
          .set_subdirectory(kFastBatchQueueSubdir)
          .set_file_prefix(kFastBatchQueuePrefix)
          .set_upload_period(kFastBatchUploadPeriod);
    case IMMEDIATE:
      return QueueOptions(*this)
          .set_subdirectory(kImmediateQueueSubdir)
          .set_file_prefix(kImmediateQueuePrefix)
          .set_upload_retry_delay(kFailedUploadRetryDelay);
    case SECURITY:
      return QueueOptions(*this)
          .set_subdirectory(kSecurityQueueSubdir)
          .set_file_prefix(kSecurityQueuePrefix)
          .set_upload_retry_delay(kFailedUploadRetryDelay)
          .set_can_shed_records(false);
    case UNDEFINED_PRIORITY:
      NOTREACHED() << "No QueueOptions for priority UNDEFINED_PRIORITY.";
  }
}

QueueOptions StorageOptions::ProduceQueueOptions(Priority priority) const {
  QueueOptions queue_options(PopulateQueueOptions(priority));
  modify_queue_options_for_tests_.Run(priority, queue_options);
  return queue_options;
}

StorageOptions::QueuesOptionsList StorageOptions::ProduceQueuesOptionsList()
    const {
  QueuesOptionsList queue_options_list;
  // Create queue option for each priority and add to the list.
  for (const auto priority : kPriorityOrder) {
    queue_options_list.emplace_back(priority, ProduceQueueOptions(priority));
  }
  return queue_options_list;
}

// static
base::span<const Priority> StorageOptions::GetPrioritiesOrder() {
  return base::make_span(kPriorityOrder);
}

QueueOptions::QueueOptions(const StorageOptions& storage_options)
    : storage_options_(storage_options) {}
QueueOptions::QueueOptions(const QueueOptions& options) = default;

}  // namespace reporting
