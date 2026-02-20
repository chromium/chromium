// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/to_string.h"

ConditionalWorkItem::ConditionalWorkItem(std::unique_ptr<Condition> condition,
                                         std::unique_ptr<WorkItem> if_item,
                                         std::unique_ptr<WorkItem> else_item)
    : condition_(std::move(condition)),
      if_item_(std::move(if_item)),
      else_item_(std::move(else_item)) {
  CHECK(condition_);
  CHECK(if_item_);
}

ConditionalWorkItem::~ConditionalWorkItem() = default;

bool ConditionalWorkItem::DoImpl() {
  condition_result_ = condition_->ShouldRun();
  VLOG(1) << "Condition for " << log_message() << " evaluated to "
          << base::ToString(condition_result_);
  if (condition_result_) {
    return if_item_->Do();
  }
  if (else_item_) {
    return else_item_->Do();
  }
  return true;
}

void ConditionalWorkItem::RollbackImpl() {
  VLOG(1) << "Rolling back conditional item " << log_message_;
  if (condition_result_) {
    if_item_->Rollback();
  } else if (else_item_) {
    else_item_->Rollback();
  }
}

// Pre-defined conditions:
//------------------------------------------------------------------------------
bool ConditionFileExists::ShouldRun() const {
  return base::PathExists(key_path_);
}

bool ConditionFileInUse::ShouldRun() const {
  // A running executable is open with exclusive write access, so attempting to
  // write to it will fail with a sharing violation. A more precise method would
  // be to open the file with DELETE access and attempt to set the delete
  // disposition on the handle. This would fail if the file was mapped into a
  // process's address space, but succeed otherwise. This seems like overkill,
  // however.
  base::File file(::CreateFile(
      file_path_.value().c_str(), FILE_WRITE_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      /*lpSecurityAttributes=*/nullptr, /*dwCreationDisposition=*/OPEN_EXISTING,
      /*dwFlagsAndAttributes=*/0, /*hTemplateFile=*/nullptr));
  if (file.IsValid()) {
    // The file could be opened for writing, so it is not in use.
    return false;
  }

  if (const auto error = ::GetLastError();
      error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
    // The file does not exist, so it cannot be in use.
    return false;
  }

  // By and large, we expect the error to be ERROR_SHARING_VIOLATION if the
  // file is being executed (see above). It may also be something like
  // ERROR_ACCESS_DENIED; e.g., if the file was deleted but open handles to it
  // remain. Consider any failure to open the file to mean that it's in-use
  // and shouldn't be replaced.
  return true;
}
