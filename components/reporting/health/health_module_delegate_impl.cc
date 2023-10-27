// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_delegate_impl.h"

#include <utility>

#include "base/logging.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"

namespace reporting {

namespace {
const size_t kRepeatedPtrFieldOverhead = 2;
}  // namespace

HealthModuleDelegateImpl::HealthModuleDelegateImpl(
    const base::FilePath& directory,
    size_t max_history_storage,
    std::string_view file_base_name)
    : directory_(directory),
      file_base_name_(file_base_name),
      max_history_storage_(max_history_storage) {}

HealthModuleDelegateImpl::~HealthModuleDelegateImpl() {
  // Because of weak ptr factory, must be on the same sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Status HealthModuleDelegateImpl::DoInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  files_ = HealthModuleFiles::Create(directory_, file_base_name_,
                                     max_history_storage_);
  if (!files_) {
    return Status(error::FAILED_PRECONDITION, "Could not create history files");
  }

  files_->PopulateHistory(&data_in_memory_);
  return Status::StatusOK();
}

void HealthModuleDelegateImpl::DoGetERPHealthData(HealthCallback cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(cb).Run(data_in_memory_);
}

void HealthModuleDelegateImpl::DoPostHealthRecord(HealthDataHistory record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (static_cast<size_t>(record.ByteSizeLong()) > max_history_storage_) {
    DVLOG(1) << "Health record exceeded max storage";
    return;
  }
  if (!IsInitialized()) {
    return;
  }

  auto* const history = data_in_memory_.mutable_history();
  size_t history_space = data_in_memory_.ByteSizeLong();
  const size_t record_space = record.ByteSizeLong();
  if (history_space + record_space > max_history_storage_) {
    size_t removable_space = 0;
    int index_removed = 0;

    // Find out how many elements must be deleted to make room.
    while (index_removed < history->size()) {
      removable_space +=
          data_in_memory_.history().Get(index_removed).ByteSizeLong() +
          kRepeatedPtrFieldOverhead;
      if (history_space + record_space - removable_space <=
          max_history_storage_) {
        break;
      }
      index_removed++;
    }
    history->DeleteSubrange(0, index_removed + 1);
  }

  storage_used_ += record.ByteSizeLong();
  *data_in_memory_.add_history() = record;
}
}  // namespace reporting
