// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_H_

#include <memory>

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem that permits conditionally executing one or another item based on
// the evaluation of a condition at runtime.
class ConditionalWorkItem : public WorkItem {
 public:
  // Constructs a WorkItem that evaluates `condition` and runs either `if_item`
  // or `else_item` (if not null) based on its result.
  ConditionalWorkItem(std::unique_ptr<Condition> condition,
                      std::unique_ptr<WorkItem> if_item,
                      std::unique_ptr<WorkItem> else_item);
  ConditionalWorkItem(const ConditionalWorkItem&) = delete;
  ConditionalWorkItem& operator=(const ConditionalWorkItem&) = delete;
  ~ConditionalWorkItem() override;

 private:
  // WorkItem:

  // Returns the result of running `if_item_` or `else_item_` depending on the
  // result of `condition_`.
  bool DoImpl() override;

  // Does a rollback of the item that was run by `DoImpl()`.
  void RollbackImpl() override;

  std::unique_ptr<Condition> condition_;
  std::unique_ptr<WorkItem> if_item_;
  std::unique_ptr<WorkItem> else_item_;
  bool condition_result_ = false;
};

// Pre-defined conditions:
//------------------------------------------------------------------------------
class ConditionFileExists : public WorkItem::Condition {
 public:
  explicit ConditionFileExists(const base::FilePath& key_path)
      : key_path_(key_path) {}

  // Returns true if the given path names a file that exists.
  bool ShouldRun() const override;

 private:
  base::FilePath key_path_;
};

class ConditionFileInUse : public WorkItem::Condition {
 public:
  explicit ConditionFileInUse(const base::FilePath& file_path)
      : file_path_(file_path) {}

  // Returns true if the given path names a file that appears to be in use in
  // the same way that an executable is when it is running.
  bool ShouldRun() const override;

 private:
  base::FilePath file_path_;
};

#endif  // CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_H_
