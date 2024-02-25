// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FOLDER_UPLOAD_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FOLDER_UPLOAD_CONFIRMATION_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

// A dialog that confirms that the user intended to upload the specific folder.
// The dialog provides information about how many files are about to be uploaded
// as well as the path to it and cautions the user to only upload to sites that
// they trust with the files. This is also a security measure against sites that
// trick a user into pressing enter, which would instantly confirm the OS folder
// picker and share the default folder selection without explicit user approval.
class FolderUploadConfirmationView : public views::DialogDelegateView {
  METADATA_HEADER(FolderUploadConfirmationView, views::DialogDelegateView)

 public:
  FolderUploadConfirmationView(
      const base::FilePath& path,
      base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)>
          callback,
      std::vector<ui::SelectedFileInfo> selected_files);
  FolderUploadConfirmationView(const FolderUploadConfirmationView&) = delete;
  FolderUploadConfirmationView& operator=(const FolderUploadConfirmationView&) =
      delete;
  ~FolderUploadConfirmationView() override;

  static views::Widget* ShowDialog(
      const base::FilePath& path,
      base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)>
          callback,
      std::vector<ui::SelectedFileInfo> selected_files,
      content::WebContents* web_contents);

  // It's really important that this dialog *does not* accept by default /
  // when a user presses enter without looking as we're looking for explicit
  // approval to share this many files with the site.
  views::View* GetInitiallyFocusedView() override;

 private:
  base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback_;
  std::vector<ui::SelectedFileInfo> selected_files_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FOLDER_UPLOAD_CONFIRMATION_VIEW_H_
