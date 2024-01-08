// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Page action icon indicating if the current page is using the File System
// Access API. Shows different icons for read access to directories and write
// access to files or directories.
class FileSystemAccessIconView : public PageActionIconView {
  METADATA_HEADER(FileSystemAccessIconView, PageActionIconView)

 public:
  FileSystemAccessIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  FileSystemAccessIconView(const FileSystemAccessIconView&) = delete;
  FileSystemAccessIconView& operator=(const FileSystemAccessIconView&) = delete;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  void OnExecuting(ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool has_write_access_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ICON_VIEW_H_
