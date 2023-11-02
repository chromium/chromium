// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item_list.h"

#include "base/files/file_util.h"
#include "base/logging.h"

ConditionalWorkItemList::ConditionalWorkItemList(Condition* condition)
    : condition_(condition) {}

ConditionalWorkItemList::~ConditionalWorkItemList() {}

bool ConditionalWorkItemList::DoImpl() {
  VLOG(1) << "Evaluating " << log_message_ << " condition...";
  if (condition_.get() && condition_->ShouldRun()) {
    VLOG(1) << "Beginning conditional work item list";
    return WorkItemList::DoImpl();
  }
  VLOG(1) << "No work to do in condition work item list " << log_message_;
  return true;
}

void ConditionalWorkItemList::RollbackImpl() {
  VLOG(1) << "Rolling back conditional list " << log_message_;
  WorkItemList::RollbackImpl();
}

// Pre-defined conditions:
//------------------------------------------------------------------------------
bool ConditionRunIfFileExists::ShouldRun() const {
  return base::PathExists(key_path_);
}

Not::Not(WorkItem::Condition* original_condition)
    : original_condition_(original_condition) {}

Not::~Not() {}

bool Not::ShouldRun() const {
  return !original_condition_->ShouldRun();
}
