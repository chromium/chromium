// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_delegate.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "components/reporting/util/file.h"

const int kRepeatedPtrFieldOverhead = 2;

namespace reporting {

HealthModuleDelegate::HealthModuleDelegate(const base::FilePath& directory,
                                           base::StringPiece file_base_name,
                                           size_t max_history_storage)
    : directory_(directory),
      file_base_name_(file_base_name),
      max_history_storage_(max_history_storage) {}

HealthModuleDelegate::~HealthModuleDelegate() = default;

void HealthModuleDelegate::Init() {
  files_ = HealthModuleFiles::Create(directory_, file_base_name_,
                                     max_history_storage_);
  if (!files_) {
    return;
  }

  files_->PopulateHistory(&data_in_memory_);
  initialized_ = true;
}

bool HealthModuleDelegate::IsInitialized() const {
  return initialized_;
}

void HealthModuleDelegate::GetERPHealthData(HealthCallback cb) const {
  std::move(cb).Run(data_in_memory_);
}

void HealthModuleDelegate::PostHealthRecord(HealthDataHistory record) {
  if (static_cast<size_t>(record.ByteSize()) > max_history_storage_) {
    DVLOG(1) << "Health record exceeded max storage";
    return;
  }
  if (!initialized_) {
    return;
  }

  auto* history = data_in_memory_.mutable_history();
  size_t history_space = data_in_memory_.ByteSize();
  const size_t record_space = record.ByteSize();
  if (history_space + record_space > max_history_storage_) {
    size_t removable_space = 0;
    int index_removed = 0;

    // Find out how many elements must be deleted to make room.
    while (index_removed < history->size()) {
      removable_space +=
          data_in_memory_.history().Get(index_removed).ByteSize() +
          kRepeatedPtrFieldOverhead;
      if (history_space + record_space - removable_space <=
          max_history_storage_) {
        break;
      }
      index_removed++;
    }
    history->DeleteSubrange(0, index_removed + 1);
  }

  storage_used_ += record.ByteSize();
  *data_in_memory_.add_history() = record;
}

base::WeakPtr<HealthModuleDelegate> HealthModuleDelegate::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace reporting
