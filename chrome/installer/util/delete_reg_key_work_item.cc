// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "chrome/installer/util/registry_util.h"

using base::win::RegKey;

DeleteRegKeyWorkItem::~DeleteRegKeyWorkItem() {}

DeleteRegKeyWorkItem::DeleteRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path,
                                           REGSAM wow64_access)
    : predefined_root_(predefined_root),
      path_(path),
      wow64_access_(wow64_access) {
  DCHECK(predefined_root);
  // It's a safe bet that we don't want to delete one of the root trees.
  DCHECK(!path.empty());
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
}

bool DeleteRegKeyWorkItem::DoImpl() {
  DCHECK(!backup_initialized_);

  if (path_.empty())
    return false;

  // Only try to make a backup if rollback is enabled.
  if (rollback_enabled()) {
    if (!backup_.Initialize(predefined_root_, path_.c_str(), wow64_access_)) {
      LOG(ERROR) << "Failed to backup destination for registry key copy.";
      return false;
    }
    backup_initialized_ = true;
  }

  // Delete the key.
  if (!installer::DeleteRegistryKey(predefined_root_, path_.c_str(),
                                    wow64_access_)) {
    return false;
  }

  return true;
}

void DeleteRegKeyWorkItem::RollbackImpl() {
  if (!backup_initialized_)
    return;

  // Delete anything in the key before restoring the backup in case someone else
  // put new data in the key after Do().
  installer::DeleteRegistryKey(predefined_root_, path_.c_str(), wow64_access_);

  // Restore the old contents.  The restoration takes on its default security
  // attributes; any custom attributes are lost.
  if (!backup_.WriteTo(predefined_root_, path_.c_str(), wow64_access_))
    LOG(ERROR) << "Failed to restore key in rollback.";
}
