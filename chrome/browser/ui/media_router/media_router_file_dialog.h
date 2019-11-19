// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/media_router/issue.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

class Browser;

namespace base {
class FilePath;
}

namespace media_router {

class MediaRouterFileDialog : public ui::SelectFileDialog::Listener {
 public:
  // The reasons that the file selection might have failed. Passed into the
  // failure callback.
  enum ValidationResult {
    FILE_OK,
    // File does not exist.
    FILE_MISSING,
    // The selected file is empty.
    FILE_EMPTY,
    // This file type is not supported by the chrome player.
    FILE_TYPE_NOT_SUPPORTED,
    // The selected file cannot be read.
    READ_FAILURE,
    // The reason for the failure is unknown.
    UNKNOWN_FAILURE,
  };

  class MediaRouterFileDialogDelegate {
   public:
    virtual ~MediaRouterFileDialogDelegate() {}

    // Called when a file is selected by the user to store the files information
    // and tell the message handler to pass along the information.
    virtual void FileDialogFileSelected(
        const ui::SelectedFileInfo& file_info) = 0;

    // Called when the file selection fails.
    virtual void FileDialogSelectionFailed(const IssueInfo& issue) = 0;

    // Called when the file selection is canceled by the user. Optionally
    // implementable.
    virtual void FileDialogSelectionCanceled() {}
  };

  // A class which defines functions to interact with the file systems.
  class FileSystemDelegate {
   public:
    FileSystemDelegate();
    virtual ~FileSystemDelegate();

    // Checks if a file exists.
    virtual bool FileExists(const base::FilePath& file_path) const;

    // Checks if a file can be read.
    virtual bool IsFileReadable(const base::FilePath& file_path) const;

    // Checks if the file type is supported in this browser.
    virtual bool IsFileTypeSupported(const base::FilePath& file_path) const;

    // Checks the size of a file, returns -1 if the file size cannot be read.
    virtual int64_t GetFileSize(const base::FilePath& file_path) const;

    // Gets the last selected directory based on the browser.
    virtual base::FilePath GetLastSelectedDirectory(Browser* browser) const;

    // Opens a dialog with |file_type_info| as the configuration, and shows
    // |default_directory| as the starting place.
    virtual void OpenFileDialog(
        ui::SelectFileDialog::Listener* listener,
        const Browser* browser,
        const base::FilePath& default_directory,
        const ui::SelectFileDialog::FileTypeInfo* file_type_info);

   private:
    // The dialog object for the file dialog. Needs to be kept in scope while
    // the dialog is open, but is not used for anything else.
    scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  };

  explicit MediaRouterFileDialog(
      base::WeakPtr<MediaRouterFileDialogDelegate> delegate);

  // Constuctor with injectable FileSystemDelegate, used for tests.
  MediaRouterFileDialog(
      base::WeakPtr<MediaRouterFileDialogDelegate> delegate,
      std::unique_ptr<FileSystemDelegate> file_system_delegate);

  ~MediaRouterFileDialog() override;

  virtual GURL GetLastSelectedFileUrl();
  virtual base::string16 GetLastSelectedFileName();

  // Checks if a file has been recorded as being selected, then attempts to
  // report interesting information about the file, such as format.
  virtual void MaybeReportLastSelectedFileInformation();

  // Opens the dialog configured to get a media file.
  virtual void OpenFileDialog(Browser* browser);

 private:
  // Reports the format of a file to the UMA stats.
  void ReportFileFormat(const base::FilePath& filename);

  // Reports the size of a file to the UMA stats.
  void ReportFileSize(const base::FilePath& filename);

  // Overridden from SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file_info,
                                 int index,
                                 void* params) override;
  void FileSelectionCanceled(void* params) override;

  // Returns a reason for failure if the file is not valid, or base::nullopt if
  // it passes validation. Has to be run on seperate thread.
  ValidationResult ValidateFile(const ui::SelectedFileInfo& file_info);

  // Takes a file info and optionally a reason for validation failure, and calls
  // the appropriate delegate function.
  void OnValidationResults(
      ui::SelectedFileInfo file_info,
      MediaRouterFileDialog::ValidationResult validation_result);

  IssueInfo CreateIssue(
      const ui::SelectedFileInfo& file_info,
      MediaRouterFileDialog::ValidationResult validation_result);

  // Used to post file operations. Cleans up after itself and cancels unrun
  // tasks when destructed.
  base::CancelableTaskTracker cancelable_task_tracker_;
  scoped_refptr<base::TaskRunner> task_runner_;

  // Pointer to the file last indicated by the system.
  base::Optional<ui::SelectedFileInfo> selected_file_;

  // The object which all file system calls go through.
  std::unique_ptr<FileSystemDelegate> file_system_delegate_;

  // Object which the media router file dialog callbacks get sent to.
  base::WeakPtr<MediaRouterFileDialogDelegate> const delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterFileDialog);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_
