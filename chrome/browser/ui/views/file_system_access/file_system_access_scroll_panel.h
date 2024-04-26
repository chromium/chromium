// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SCROLL_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SCROLL_PANEL_H_

#include "base/files/file_path.h"
#include "ui/views/controls/scroll_view.h"

// Scrollable panel that displays a list of file paths, used in File System
// Access API UI surfaces.
//
// TODO(crbug.com/40101962): This UI is still in progress and missing correct
// styles, accessibility support, etc.
class FileSystemAccessScrollPanel {
 public:
  static std::unique_ptr<views::ScrollView> Create(
      const std::vector<base::FilePath>& file_paths);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SCROLL_PANEL_H_
