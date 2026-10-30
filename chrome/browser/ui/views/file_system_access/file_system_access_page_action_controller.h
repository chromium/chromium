// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class Page;
}

namespace tabs {
class TabInterface;
}

class FileSystemAccessPageActionController
    : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(FileSystemAccessPageActionController);

  explicit FileSystemAccessPageActionController(
      tabs::TabInterface& tab_interface);
  ~FileSystemAccessPageActionController() override;

  FileSystemAccessPageActionController(
      const FileSystemAccessPageActionController&) = delete;
  FileSystemAccessPageActionController& operator=(
      const FileSystemAccessPageActionController&) = delete;

  static FileSystemAccessPageActionController* From(tabs::TabInterface* tab);

  // Updates the visibility of the File System Access page action icon.
  void UpdateVisibility();

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Hides the File System Access page action icon.
  void HideIcon();

  ui::ScopedUnownedUserData<FileSystemAccessPageActionController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PAGE_ACTION_CONTROLLER_H_
