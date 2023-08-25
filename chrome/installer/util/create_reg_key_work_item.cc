// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "chrome/installer/util/create_reg_key_work_item.h"

using base::win::RegKey;

namespace {

// TODO: refactor this because it is only used once.
void UpOneDirectoryOrEmpty(std::wstring* dir) {
  base::FilePath path = base::FilePath(*dir);
  base::FilePath directory = path.DirName();
  // If there is no separator, we will get back kCurrentDirectory.
  // In this case, clear dir.
  if (directory == path ||
      directory.value() == base::FilePath::kCurrentDirectory)
    dir->clear();
  else
    *dir = directory.value();
}

}  // namespace

CreateRegKeyWorkItem::~CreateRegKeyWorkItem() {}

CreateRegKeyWorkItem::CreateRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path,
                                           REGSAM wow64_access)
    : predefined_root_(predefined_root),
      path_(path),
      wow64_access_(wow64_access),
      key_created_(false) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
}

bool CreateRegKeyWorkItem::DoImpl() {
  if (!InitKeyList()) {
    // Nothing needs to be done here.
    VLOG(1) << "no key to create";
    return true;
  }

  RegKey key;
  std::wstring key_path;

  // To create keys, we iterate from back to front.
  for (size_t i = key_list_.size(); i > 0; i--) {
    DWORD disposition;
    key_path.assign(key_list_[i - 1]);

    if (key.CreateWithDisposition(predefined_root_, key_path.c_str(),
                                  &disposition,
                                  KEY_READ | wow64_access_) == ERROR_SUCCESS) {
      if (disposition == REG_OPENED_EXISTING_KEY) {
        if (key_created_) {
          // This should not happen. Someone created a subkey under the key
          // we just created?
          LOG(ERROR) << key_path << " exists, this is not expected.";
          return false;
        }
        // Remove the key path from list if it is already present.
        key_list_.pop_back();
      } else if (disposition == REG_CREATED_NEW_KEY) {
        VLOG(1) << "created " << key_path;
        key_created_ = true;
      } else {
        LOG(ERROR) << "unkown disposition";
        return false;
      }
    } else {
      LOG(ERROR) << "Failed to create " << key_path;
      return false;
    }
  }

  return true;
}

void CreateRegKeyWorkItem::RollbackImpl() {
  if (!key_created_)
    return;

  // To delete keys, we iterate from front to back.
  for (const auto& key_path : key_list_) {
    RegKey key(predefined_root_, key_path.c_str(),
               KEY_QUERY_VALUE | wow64_access_);
    if (key.GetValueCount().value_or(1) == 0 &&
        key.DeleteKey(L"", base::win::RegKey::RecursiveDelete(false)) ==
            ERROR_SUCCESS) {
      VLOG(1) << "rollback: delete " << key_path;
    } else {
      VLOG(1) << "rollback: can not delete " << key_path;
      // The key might have been deleted, but we don't reliably know what
      // error code(s) are returned in this case. So we just keep tring delete
      // the rest.
    }
  }

  key_created_ = false;
  key_list_.clear();
  return;
}

bool CreateRegKeyWorkItem::InitKeyList() {
  if (path_.empty())
    return false;

  std::wstring key_path(path_);

  do {
    key_list_.push_back(key_path);
    // This is pure string operation so it does not matter whether the
    // path is file path or registry path.
    UpOneDirectoryOrEmpty(&key_path);
  } while (!key_path.empty());

  return true;
}
