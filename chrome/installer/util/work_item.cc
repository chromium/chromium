// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/work_item.h"

#include <windows.h>

#include "base/check_op.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/delete_reg_value_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item_list.h"

WorkItem::WorkItem() = default;
WorkItem::~WorkItem() = default;

CallbackWorkItem* WorkItem::CreateCallbackWorkItem(
    base::OnceCallback<bool(const CallbackWorkItem&)> do_action,
    base::OnceCallback<void(const CallbackWorkItem&)> rollback_action) {
  return new CallbackWorkItem(std::move(do_action), std::move(rollback_action));
}

CopyTreeWorkItem* WorkItem::CreateCopyTreeWorkItem(
    const base::FilePath& source_path,
    const base::FilePath& dest_path,
    const base::FilePath& temp_path,
    CopyOverWriteOption overwrite_option,
    const base::FilePath& alternative_path) {
  return new CopyTreeWorkItem(source_path, dest_path, temp_path,
                              overwrite_option, alternative_path);
}

CreateDirWorkItem* WorkItem::CreateCreateDirWorkItem(
    const base::FilePath& path) {
  return new CreateDirWorkItem(path);
}

CreateRegKeyWorkItem* WorkItem::CreateCreateRegKeyWorkItem(
    HKEY predefined_root,
    const std::wstring& path,
    REGSAM wow64_access) {
  return new CreateRegKeyWorkItem(predefined_root, path, wow64_access);
}

DeleteRegKeyWorkItem* WorkItem::CreateDeleteRegKeyWorkItem(
    HKEY predefined_root,
    const std::wstring& path,
    REGSAM wow64_access) {
  return new DeleteRegKeyWorkItem(predefined_root, path, wow64_access);
}

DeleteRegValueWorkItem* WorkItem::CreateDeleteRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name) {
  return new DeleteRegValueWorkItem(predefined_root, key_path, wow64_access,
                                    value_name);
}

DeleteTreeWorkItem* WorkItem::CreateDeleteTreeWorkItem(
    const base::FilePath& root_path,
    const base::FilePath& temp_path) {
  return new DeleteTreeWorkItem(root_path, temp_path);
}

MoveTreeWorkItem* WorkItem::CreateMoveTreeWorkItem(
    const base::FilePath& source_path,
    const base::FilePath& dest_path,
    const base::FilePath& temp_path,
    MoveTreeOption duplicate_option) {
  return new MoveTreeWorkItem(source_path, dest_path, temp_path,
                              duplicate_option);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    const std::wstring& value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path, wow64_access,
                                 value_name, value_data, overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    DWORD value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path, wow64_access,
                                 value_name, value_data, overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    int64_t value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path, wow64_access,
                                 value_name, value_data, overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    GetValueFromExistingCallback get_value_callback) {
  return new SetRegValueWorkItem(predefined_root, key_path, wow64_access,
                                 value_name, std::move(get_value_callback));
}

WorkItemList* WorkItem::CreateWorkItemList() {
  return new WorkItemList();
}

WorkItemList* WorkItem::CreateConditionalWorkItemList(Condition* condition) {
  return new ConditionalWorkItemList(condition);
}

bool WorkItem::Do() {
  DCHECK_EQ(BEFORE_DO, state_);
  const bool success = DoImpl();
  state_ = AFTER_DO;
  return best_effort() ? true : success;
}

void WorkItem::Rollback() {
  DCHECK_EQ(AFTER_DO, state_);
  if (rollback_enabled())
    RollbackImpl();
  state_ = AFTER_ROLLBACK;
}

void WorkItem::set_best_effort(bool best_effort) {
  DCHECK_EQ(BEFORE_DO, state());
  best_effort_ = best_effort;
}

void WorkItem::set_rollback_enabled(bool rollback_enabled) {
  DCHECK_EQ(BEFORE_DO, state());
  rollback_enabled_ = rollback_enabled;
}
