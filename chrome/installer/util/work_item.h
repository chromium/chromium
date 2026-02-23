// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Base class for managing an action of a sequence of actions to be carried
// out during install/update/uninstall. Supports rollback of actions if this
// process fails.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_H_

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

class CallbackWorkItem;
class CopyTreeWorkItem;
class CreateDirWorkItem;
class CreateRegKeyWorkItem;
class DeleteTreeWorkItem;
class DeleteRegKeyWorkItem;
class DeleteRegValueWorkItem;
class MoveTreeWorkItem;
class SetRegValueWorkItem;
class WorkItemList;

namespace base {
class FilePath;
}

// A base class that defines APIs to perform/rollback an action or a
// sequence of actions during install/update/uninstall.
//
// Subclasses must implement DoImpl() and RollbackImpl().
//
// Do() calls DoImpl(). When the "best-effort" flag is true, it always returns
// true. Otherwise, it returns the value returned by DoImpl(). Implementations
// of DoImpl() may use the "best-effort" flag as an indication that they should
// do as much work as possible (i.e., shouldn't abort as soon as one of the
// actions they have to perform fails). By default, the "best-effort" flag is
// false.
//
// Rollback() calls RollbackImpl() when the "rollback enabled" flag is true.
// Otherwise, it's a no-op. Implementations of DoImpl() may read the "rollback
// enabled" flag to determine whether they should save state to support
// rollback. By default, the "rollback enabled" flag is true.
//
// The "best-effort" and "rollback enabled" must be set before Do() is invoked.
class WorkItem {
 public:
  // A callback that returns the desired value based on the |existing_value|.
  // |existing_value| will be empty if the value didn't previously exist or
  // existed under a non-string type.
  using GetValueFromExistingCallback =
      base::OnceCallback<std::wstring(const std::wstring& existing_value)>;

  // All registry operations can be instructed to operate on a specific view
  // of the registry by specifying a REGSAM value to the wow64_access parameter.
  // The wow64_access parameter can be one of:
  // KEY_WOW64_32KEY - Operate on the 32-bit view.
  // KEY_WOW64_64KEY - Operate on the 64-bit view.
  // kWow64Default   - Operate on the default view (e.g. 32-bit on 32-bit
  //                   systems, and 64-bit on 64-bit systems).
  // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx
  static const REGSAM kWow64Default = 0;

  // Abstract base class for the conditions used by ConditionWorkItemList.
  // TODO(robertshield): Move this out of WorkItem.
  class Condition {
   public:
    virtual ~Condition() = default;
    virtual bool ShouldRun() const = 0;
  };

  virtual ~WorkItem();

  // Create a CallbackWorkItem that invokes a callback.
  static CallbackWorkItem* CreateCallbackWorkItem(
      base::OnceCallback<bool(const CallbackWorkItem&)> do_action,
      base::OnceCallback<void(const CallbackWorkItem&)> rollback_action);

  // Create a CopyTreeWorkItem that recursively copies a file system hierarchy
  // from source path to destination path.
  static CopyTreeWorkItem* CreateCopyTreeWorkItem(
      const base::FilePath& source_path,
      const base::FilePath& dest_path,
      const base::FilePath& temp_path);

  // Create a CreateDirWorkItem that creates a directory at the given path.
  static CreateDirWorkItem* CreateCreateDirWorkItem(const base::FilePath& path);

  // Create a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  static CreateRegKeyWorkItem* CreateCreateRegKeyWorkItem(
      HKEY predefined_root,
      const std::wstring& path,
      REGSAM wow64_access);

  // Create a DeleteRegKeyWorkItem that deletes a registry key at the given
  // path.
  static DeleteRegKeyWorkItem* CreateDeleteRegKeyWorkItem(
      HKEY predefined_root,
      const std::wstring& path,
      REGSAM wow64_access);

  // Create a DeleteRegValueWorkItem that deletes a registry value
  static DeleteRegValueWorkItem* CreateDeleteRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name);

  // Create a DeleteTreeWorkItem that recursively deletes a file system
  // hierarchy at the given root path.
  static DeleteTreeWorkItem* CreateDeleteTreeWorkItem(
      const base::FilePath& root_path,
      const base::FilePath& temp_path);

  // Create a MoveTreeWorkItem that recursively moves a file system hierarchy
  // from source path to destination path.
  struct MoveTreeOptions {
    // If true, only move if dest_path is different.
    bool check_for_duplicates = false;
    // If true, do not fail if source_path cannot be fully deleted.
    bool lenient_deletion = false;
  };
  static MoveTreeWorkItem* CreateMoveTreeWorkItem(
      const base::FilePath& source_path,
      const base::FilePath& dest_path,
      const base::FilePath& temp_path,
      MoveTreeOptions options);

  // Create a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      const std::wstring& value_data,
      bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      DWORD value_data,
      bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value with REG_QWORD type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      int64_t value_data,
      bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value based on the value
  // provided by |get_value_callback| given the existing value under
  // |key_path\value_name|.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      GetValueFromExistingCallback get_value_callback);

  // Create an empty WorkItemList. A WorkItemList can recursively contains
  // a list of WorkItems.
  static WorkItemList* CreateWorkItemList();

  // Create a conditional work item that will execute either `if_item` or
  // `else_item` based on the result of `condition->ShouldRun()`.
  static WorkItem* CreateConditionalWorkItem(
      std::unique_ptr<Condition> condition,
      std::unique_ptr<WorkItem> if_item,
      std::unique_ptr<WorkItem> else_item);

  // Perform the actions of WorkItem. Returns true if success or if
  // best_effort(). Can only be called once per instance.
  bool Do();

  // Rollback any actions previously carried out by this WorkItem if
  // rollback_enabled(). Can only be called once per instance, after Do() has
  // returned.
  void Rollback();

  void set_best_effort(bool best_effort);
  bool best_effort() const { return best_effort_; }
  void set_rollback_enabled(bool rollback_enabled);
  bool rollback_enabled() const { return rollback_enabled_; }

  // Sets an optional log message that a work item may use to print additional
  // instance-specific information.
  void set_log_message(const std::string& log_message) {
    log_message_ = log_message;
  }

  // Retrieves the optional log message. The retrieved string may be empty.
  const std::string& log_message() const { return log_message_; }

 protected:
  enum State {
    BEFORE_DO,
    AFTER_DO,
    AFTER_ROLLBACK,
  };

  WorkItem();

  State state() const { return state_; }

  std::string log_message_;

 private:
  // Called by Do(). Performs the actions of the Workitem.
  virtual bool DoImpl() = 0;

  // Called by Rollback() if rollback_enabled() is true. Rollbacks the actions
  // performed by DoImpl(). Implementations must support invocation of this even
  // when DoImpl() returned false.
  virtual void RollbackImpl() = 0;

  State state_ = BEFORE_DO;
  bool best_effort_ = false;
  bool rollback_enabled_ = true;
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_H_
