// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_

#include <windows.h>

#include <string>

#include "chrome/installer/util/registry_key_backup.h"
#include "chrome/installer/util/work_item.h"

class RegistryKeyBackup;

// A WorkItem subclass that deletes a registry key at the given path.  Be aware
// that in the event of rollback the key's values and subkeys are restored but
// the key and its subkeys take on their default security descriptors.
class DeleteRegKeyWorkItem : public WorkItem {
 public:
  DeleteRegKeyWorkItem(const DeleteRegKeyWorkItem&) = delete;
  DeleteRegKeyWorkItem& operator=(const DeleteRegKeyWorkItem&) = delete;

  ~DeleteRegKeyWorkItem() override;

 private:
  friend class WorkItem;

  DeleteRegKeyWorkItem(HKEY predefined_root,
                       const std::wstring& path,
                       REGSAM wow64_access);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Root key from which we delete the key. The root key can only be
  // one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the key to be deleted.
  std::wstring path_;

  // Whether to force 32-bit or 64-bit view of the target key.
  REGSAM wow64_access_;

  // Backup of the deleted key.
  RegistryKeyBackup backup_;

  // True if |backup_| has been initialized.
  bool backup_initialized_ = false;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_
