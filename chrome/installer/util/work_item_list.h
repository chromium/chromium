// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_

#include <windows.h>

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively contains a list of WorkItems. Thus it
// provides functionalities to carry out or roll back the sequence of actions
// defined by the list of WorkItems it contains.
// The WorkItems are executed in the same order as they are added to the list.
// The "best-effort" flag of the WorkItemList is propagated to the WorkItems
// when it's true. Likewise, the "rollback enabled" flag of the WorkItemList is
// propagated to the WorkItems when it's false.
class WorkItemList : public WorkItem {
 public:
  ~WorkItemList() override;

  // Add a WorkItem to the list.
  // A WorkItem can only be added to the list before the list's DO() is called.
  // Once a WorkItem is added to the list. The list owns the WorkItem.
  virtual void AddWorkItem(WorkItem* work_item);

  // Add a CallbackWorkItem that invokes a callback.
  virtual WorkItem* AddCallbackWorkItem(
      base::OnceCallback<bool(const CallbackWorkItem&)> do_action,
      base::OnceCallback<void(const CallbackWorkItem&)> rollback_action);

  // Add a CopyTreeWorkItem to the list of work items.
  // See the NOTE in the documentation for the CopyTreeWorkItem class for
  // special considerations regarding |temp_path|.
  virtual WorkItem* AddCopyTreeWorkItem(
      const base::FilePath& source_path,
      const base::FilePath& dest_path,
      const base::FilePath& temp_path,
      CopyOverWriteOption overwrite_option,
      const base::FilePath& alternative_path = base::FilePath());

  // Add a CreateDirWorkItem that creates a directory at the given path.
  virtual WorkItem* AddCreateDirWorkItem(const base::FilePath& path);

  // Add a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  virtual WorkItem* AddCreateRegKeyWorkItem(HKEY predefined_root,
                                            const std::wstring& path,
                                            REGSAM wow64_access);

  // Add a DeleteRegKeyWorkItem that deletes a registry key from the given
  // path.
  virtual WorkItem* AddDeleteRegKeyWorkItem(HKEY predefined_root,
                                            const std::wstring& path,
                                            REGSAM wow64_access);

  // Add a DeleteRegValueWorkItem that deletes registry value of type REG_SZ
  // or REG_DWORD.
  virtual WorkItem* AddDeleteRegValueWorkItem(HKEY predefined_root,
                                              const std::wstring& key_path,
                                              REGSAM wow64_access,
                                              const std::wstring& value_name);

  // Add a DeleteTreeWorkItem that recursively deletes a file system hierarchy
  // at the given root path.
  virtual WorkItem* AddDeleteTreeWorkItem(const base::FilePath& root_path,
                                          const base::FilePath& temp_path);

  // Add a MoveTreeWorkItem to the list of work items.
  virtual WorkItem* AddMoveTreeWorkItem(const base::FilePath& source_path,
                                        const base::FilePath& dest_path,
                                        const base::FilePath& temp_path,
                                        MoveTreeOption duplicate_option);

  // Add a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           const std::wstring& value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           DWORD value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value with REG_QWORD type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           int64_t value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value based on the value
  // provided by |get_value_callback| given the existing value under
  // |key_path\value_name|.
  virtual WorkItem* AddSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      WorkItem::GetValueFromExistingCallback get_value_callback);

 protected:
  friend class WorkItem;

  typedef std::list<raw_ptr<WorkItem, CtnExperimental>> WorkItems;
  typedef WorkItems::iterator WorkItemIterator;

  WorkItemList();

  // WorkItem:

  // Execute the WorkItems in the same order as they are added to the list. It
  // aborts as soon as one WorkItem fails, unless the best-effort flag is true.
  bool DoImpl() override;

  // Rollback the WorkItems in the reverse order as they are executed.
  void RollbackImpl() override;

  // The list of WorkItems, in the order of them being added.
  WorkItems list_;

  // The list of executed WorkItems, in the reverse order of them being
  // executed.
  WorkItems executed_list_;
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_
