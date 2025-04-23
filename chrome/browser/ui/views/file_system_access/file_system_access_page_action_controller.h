// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"

namespace tabs {
class TabInterface;
}

class FileSystemAccessPageActionController {
 public:
  explicit FileSystemAccessPageActionController(
      tabs::TabInterface& tab_interface);
  ~FileSystemAccessPageActionController() = default;

  FileSystemAccessPageActionController(
      const FileSystemAccessPageActionController&) = delete;
  FileSystemAccessPageActionController& operator=(
      const FileSystemAccessPageActionController&) = delete;

  // Updates the visibility of the File System Access page action icon.
  void UpdateVisibility();

 private:
  // Hides the File System Access page action icon.
  void HideIcon();

  const raw_ref<tabs::TabInterface> tab_interface_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_
