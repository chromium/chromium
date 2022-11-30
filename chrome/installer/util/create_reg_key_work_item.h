// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H_

#include <windows.h>

#include <string>
#include <vector>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that creates a registry key at the given path.
// It also creates all necessary intermediate keys if they do not exist.
class CreateRegKeyWorkItem : public WorkItem {
 public:
  ~CreateRegKeyWorkItem() override;

 private:
  friend class WorkItem;

  CreateRegKeyWorkItem(HKEY predefined_root,
                       const std::wstring& path,
                       REGSAM wow64_access);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Initialize key_list_ by adding all paths of keys from predefined_root_
  // to path_. Returns true if key_list_ is non empty.
  bool InitKeyList();

  // Root key under which we create the new key. The root key can only be
  // one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the key to be created.
  std::wstring path_;

  // Whether to force 32-bit or 64-bit view of the target key.
  REGSAM wow64_access_;

  // List of paths to all keys that need to be created from predefined_root_
  // to path_.
  std::vector<std::wstring> key_list_;

  // Whether any key has been created.
  bool key_created_;
};

#endif  // CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H_
