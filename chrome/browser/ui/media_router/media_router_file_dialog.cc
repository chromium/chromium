// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_file_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/container_names.h"
#include "media/base/mime_util.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

namespace {

base::string16 GetFileName(const ui::SelectedFileInfo& file_info) {
  return file_info.file_path.BaseName().LossyDisplayName();
}

// Returns info about extensions for files we support as audio video files.
ui::SelectFileDialog::FileTypeInfo GetAudioVideoFileTypeInfo() {
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  file_type_info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  std::vector<base::FilePath::StringType> extensions;

  // Add all extensions from the audio and video mime types.
  net::GetExtensionsForMimeType("video/*", &extensions);
  net::GetExtensionsForMimeType("audio/*", &extensions);

  // Filter based on what can be played on the media player
  std::vector<base::FilePath::StringType> filtered_extensions;
  std::copy_if(extensions.begin(), extensions.end(),
               std::back_inserter(filtered_extensions),
               [](const base::FilePath::StringType& extension) {
                 std::string mime_type;
                 net::GetWellKnownMimeTypeFromExtension(extension, &mime_type);
                 return media::IsSupportedMediaMimeType(mime_type);
               });

  // Add all audio and video extensions as a single type to the dialog.
  file_type_info.extensions.push_back(filtered_extensions);

  // Set the description, otherwise it lists all the possible extensions which
  // looks bad.
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_FILE_DIALOG_AUDIO_VIDEO_FILTER));

  // Add an option for all files
  file_type_info.include_all_files = true;

  return file_type_info;
}

}  // namespace

MediaRouterFileDialog::FileSystemDelegate::FileSystemDelegate() = default;

MediaRouterFileDialog::FileSystemDelegate::~FileSystemDelegate() {
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

bool MediaRouterFileDialog::FileSystemDelegate::FileExists(
    const base::FilePath& file_path) const {
  // We assume if the path exists, the file exists.
  return base::PathExists(file_path);
}

bool MediaRouterFileDialog::FileSystemDelegate::IsFileReadable(
    const base::FilePath& file_path) const {
  char buffer[1];
  return base::ReadFile(file_path, buffer, 1) != -1;
}

bool MediaRouterFileDialog::FileSystemDelegate::IsFileTypeSupported(
    const base::FilePath& file_path) const {
  std::string mime_type;
  net::GetMimeTypeFromFile(file_path, &mime_type);
  return media::IsSupportedMediaMimeType(mime_type);
}

int64_t MediaRouterFileDialog::FileSystemDelegate::GetFileSize(
    const base::FilePath& file_path) const {
  int64_t file_size;
  return base::GetFileSize(file_path, &file_size) ? file_size : -1;
}

base::FilePath
MediaRouterFileDialog::FileSystemDelegate::GetLastSelectedDirectory(
    Browser* browser) const {
  return browser->profile()->last_selected_directory();
}

void MediaRouterFileDialog::FileSystemDelegate::OpenFileDialog(
    ui::SelectFileDialog::Listener* listener,
    const Browser* browser,
    const base::FilePath& default_directory,
    const ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      listener, std::make_unique<ChromeSelectFilePolicy>(
                    browser->tab_strip_model()->GetActiveWebContents()));

  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(),
      default_directory, file_type_info, 0, base::FilePath::StringType(),
      parent_window, nullptr /* |params| passed to the listener */);
}
// End of FileSystemDelegate default implementations

MediaRouterFileDialog::MediaRouterFileDialog(
    base::WeakPtr<MediaRouterFileDialogDelegate> delegate)
    : MediaRouterFileDialog(std::move(delegate),
                            std::make_unique<FileSystemDelegate>()) {}

// Used for tests
MediaRouterFileDialog::MediaRouterFileDialog(
    base::WeakPtr<MediaRouterFileDialogDelegate> delegate,
    std::unique_ptr<FileSystemDelegate> file_system_delegate)
    : task_runner_(base::CreateTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})),
      file_system_delegate_(std::move(file_system_delegate)),
      delegate_(std::move(delegate)) {}

MediaRouterFileDialog::~MediaRouterFileDialog() = default;

GURL MediaRouterFileDialog::GetLastSelectedFileUrl() {
  return selected_file_.has_value()
             ? net::FilePathToFileURL(selected_file_->local_path)
             : GURL();
}

base::string16 MediaRouterFileDialog::GetLastSelectedFileName() {
  return selected_file_.has_value() ? GetFileName(selected_file_.value())
                                    : base::string16();
}

void MediaRouterFileDialog::MaybeReportLastSelectedFileInformation() {
  if (selected_file_.has_value()) {
    cancelable_task_tracker_.PostTask(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&MediaRouterFileDialog::ReportFileFormat,
                       base::Unretained(this), selected_file_->local_path));

    cancelable_task_tracker_.PostTask(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&MediaRouterFileDialog::ReportFileSize,
                       base::Unretained(this), selected_file_->local_path));
  } else {
    VLOG(1) << "MediaRouterFileDialog did not report file information; no file "
               "to report.";
  }
}

void MediaRouterFileDialog::OpenFileDialog(Browser* browser) {
  const base::FilePath directory =
      file_system_delegate_->GetLastSelectedDirectory(browser);

  const ui::SelectFileDialog::FileTypeInfo file_type_info =
      GetAudioVideoFileTypeInfo();

  file_system_delegate_->OpenFileDialog(this, browser, directory,
                                        &file_type_info);
}

void MediaRouterFileDialog::ReportFileFormat(const base::FilePath& filename) {
  // Windows implementation of ReadFile fails if file smaller than desired size,
  // so use file length if file less than 8192 bytes (http://crbug.com/243885).
  char buffer[8192];
  int read_size = sizeof(buffer);
  int64_t actual_size;
  if (base::GetFileSize(filename, &actual_size) && actual_size < read_size)
    read_size = actual_size;
  int read = base::ReadFile(filename, buffer, read_size);

  MediaRouterMetrics::RecordMediaRouterFileFormat(
      media::container_names::DetermineContainer(
          reinterpret_cast<const uint8_t*>(buffer), read));
}

void MediaRouterFileDialog::ReportFileSize(const base::FilePath& filename) {
  int64_t size;
  if (base::GetFileSize(filename, &size)) {
    MediaRouterMetrics::RecordMediaRouterFileSize(size);
  } else {
    VLOG(1) << "Can't get file size for file: " << filename.LossyDisplayName()
            << ".";
  }
}

void MediaRouterFileDialog::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void MediaRouterFileDialog::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file_info,
    int index,
    void* params) {
  cancelable_task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaRouterFileDialog::ValidateFile,
                     base::Unretained(this), file_info),
      base::BindOnce(&MediaRouterFileDialog::OnValidationResults,
                     base::Unretained(this), file_info));
}

void MediaRouterFileDialog::OnValidationResults(
    ui::SelectedFileInfo file_info,
    MediaRouterFileDialog::ValidationResult validation_result) {
  if (validation_result == MediaRouterFileDialog::FILE_OK) {
    selected_file_ = file_info;
    if (delegate_)
      delegate_->FileDialogFileSelected(file_info);
  } else if (delegate_) {
    delegate_->FileDialogSelectionFailed(
        CreateIssue(file_info, validation_result));
  }
}

void MediaRouterFileDialog::FileSelectionCanceled(void* params) {
  if (delegate_)
    delegate_->FileDialogSelectionCanceled();
}

IssueInfo MediaRouterFileDialog::CreateIssue(
    const ui::SelectedFileInfo& file_info,
    MediaRouterFileDialog::ValidationResult validation_result) {
  std::string issue_title;
  switch (validation_result) {
    case MediaRouterFileDialog::FILE_MISSING:
    case MediaRouterFileDialog::FILE_EMPTY:
    case MediaRouterFileDialog::FILE_TYPE_NOT_SUPPORTED:
    case MediaRouterFileDialog::READ_FAILURE:
      issue_title = l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_ISSUE_FILE_CAST_ERROR, GetFileName(file_info));
      break;
    case MediaRouterFileDialog::FILE_OK:
      // Create issue shouldn't be called with FILE_OK, but to ensure things
      // compile, fall through sets |issue_title| to the generic error.
      NOTREACHED();
      FALLTHROUGH;
    case MediaRouterFileDialog::UNKNOWN_FAILURE:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_FILE_CAST_GENERIC_ERROR);
      break;
  }
  return IssueInfo(issue_title, IssueInfo::Action::DISMISS,
                   IssueInfo::Severity::WARNING);
}

MediaRouterFileDialog::ValidationResult MediaRouterFileDialog::ValidateFile(
    const ui::SelectedFileInfo& file_info) {
  // Attempt to determine if file exsists.
  if (!file_system_delegate_->FileExists(file_info.local_path))
    return MediaRouterFileDialog::FILE_MISSING;

  // Attempt to read the file size and verify that the file has contents.
  int file_size = file_system_delegate_->GetFileSize(file_info.local_path);
  if (file_size < 0)
    return MediaRouterFileDialog::READ_FAILURE;

  if (file_size == 0)
    return MediaRouterFileDialog::FILE_EMPTY;

  if (!file_system_delegate_->IsFileReadable(file_info.local_path))
    return MediaRouterFileDialog::READ_FAILURE;

  if (!file_system_delegate_->IsFileTypeSupported(file_info.local_path))
    return MediaRouterFileDialog::FILE_TYPE_NOT_SUPPORTED;

  return MediaRouterFileDialog::FILE_OK;
}

}  // namespace media_router
