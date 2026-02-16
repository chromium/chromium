// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item.h"

#include <utility>

#include "base/check.h"
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
