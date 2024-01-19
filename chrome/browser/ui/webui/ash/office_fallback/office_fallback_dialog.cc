// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace {

// Width of the Fallback dialog as found with the inspector tool.
const int kWidth = 512;

// Height of the Fallback dialogs for different text lengths as found with the
// inspector tool.
const int kOfflineHeight = 264;
const int kDriveUnavailableHeight = 244;
const int kMeteredHeight = 264;

// Return the task title id for the task represented by the `action_id`.
int GetTaskTitleId(const std::string& action_id) {
  if (action_id == file_manager::file_tasks::kActionIdWebDriveOfficeWord) {
    return IDS_FILE_BROWSER_TASK_OPEN_GDOC;
  } else if (action_id ==
             file_manager::file_tasks::kActionIdWebDriveOfficeExcel) {
    return IDS_FILE_BROWSER_TASK_OPEN_GSHEET;
  } else if (action_id ==
             file_manager::file_tasks::kActionIdWebDriveOfficePowerPoint) {
    return IDS_FILE_BROWSER_TASK_OPEN_GSLIDES;
  } else if (action_id == file_manager::file_tasks::kActionIdOpenInOffice) {
    return IDS_FILE_BROWSER_TASK_OPEN_MICROSOFT_365;
  }
  // TODO(cassycc): add test for this path.
  LOG(ERROR) << "Could not find a task with the given action_id";
  NOTREACHED();
  return 0;
}

// Get the text ids for the `fallback_reason` specific translated strings that
// will be displayed in dialog. Store them in the out parameters `title_id`,
// `reason_message_id` and `instructions_message_id`. Get the corresponding
// width and height needed to display these strings in the dialog. Store them in
// the out parameters `width` and `height`.
void GetDialogTextIdsAndSize(
    const ash::office_fallback::FallbackReason fallback_reason,
    int& title_id,
    int& reason_message_id,
    int& instructions_message_id,
    int& width,
    int& height) {
  width = kWidth;
  switch (fallback_reason) {
    case ash::office_fallback::FallbackReason::kOffline:
      title_id = IDS_OFFICE_FALLBACK_TITLE_OFFLINE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_OFFLINE;
      instructions_message_id = IDS_OFFICE_FALLBACK_INSTRUCTIONS_OFFLINE;
      height = kOfflineHeight;
      break;
    case ash::office_fallback::FallbackReason::kDriveDisabled:
    case ash::office_fallback::FallbackReason::kNoDriveService:
    case ash::office_fallback::FallbackReason::kDriveAuthenticationNotReady:
    case ash::office_fallback::FallbackReason::kDriveFsInterfaceError:
      title_id = IDS_OFFICE_FALLBACK_TITLE_DRIVE_UNAVAILABLE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_DRIVE_UNAVAILABLE;
      instructions_message_id =
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_DRIVE_UNAVAILABLE;
      height = kDriveUnavailableHeight;
      break;
    case ash::office_fallback::FallbackReason::kMeteredConnection:
      title_id = IDS_OFFICE_FALLBACK_TITLE_METERED;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_METERED;
      instructions_message_id = IDS_OFFICE_FALLBACK_INSTRUCTIONS_METERED;
      height = kMeteredHeight;
      break;
  }
}
}  // namespace

namespace ash::office_fallback {

// static
bool OfficeFallbackDialog::Show(
    const std::vector<storage::FileSystemURL>& file_urls,
    FallbackReason fallback_reason,
    const std::string& action_id,
    DialogChoiceCallback callback) {
  // Allow no more than one office fallback dialog at a time. In the case of
  // multiple dialog requests, they should either be handled simultaneously or
  // queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUIOfficeFallbackURL))) {
    LOG(WARNING) << "Another fallback dialog is already being shown";
    std::move(callback).Run(std::nullopt);
    return false;
  }

  DCHECK(!file_urls.empty());
  if (file_urls.empty()) {
    LOG(ERROR) << "No file urls";
    std::move(callback).Run(std::nullopt);
    return false;
  }

  // TODO(b/242685536) When multi-file selection is defined, display file names
  // appropriately. Currently, file_urls_ is just a singleton array.
  // TODO(cassycc): Handle long file name(s).
  // Get file name to display.
  const std::u16string file_name(
      file_urls.front().path().BaseName().LossyDisplayName());

  // Get title of task which fails to open file.
  int task_title_id = GetTaskTitleId(action_id);
  if (task_title_id == 0) {
    LOG(WARNING) << "No task_title_id from action_id";
    std::move(callback).Run(std::nullopt);
    return false;
  }
  const std::u16string task_title = l10n_util::GetStringUTF16(task_title_id);

  // Get failure specific text to display in dialog.
  int title_id;
  int reason_message_id;
  int instructions_message_id;
  int width;
  int height;
  GetDialogTextIdsAndSize(fallback_reason, title_id, reason_message_id,
                          instructions_message_id, width, height);
  // TODO(cassycc): Figure out how to add the web_drive to the placeholder in
  // IDS_OFFICE_FALLBACK_TITLE_WEB_DRIVE_UNAVAILABLE.
  const std::string title_text = l10n_util::GetStringFUTF8(title_id, file_name);
  const std::string reason_message =
      fallback_reason ==
              ash::office_fallback::FallbackReason::kMeteredConnection
          ? l10n_util::GetStringUTF8(reason_message_id)
          : l10n_util::GetStringFUTF8(reason_message_id, task_title);
  const std::string instructions_message =
      l10n_util::GetStringUTF8(instructions_message_id);

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  OfficeFallbackDialog* dialog = new OfficeFallbackDialog(
      file_urls, title_text, reason_message, instructions_message, width,
      height, std::move(callback));

  dialog->ShowSystemDialog();
  return true;
}

void OfficeFallbackDialog::OnDialogClosed(const std::string& choice) {
  // Save callback as local variable before member variables are deleted during
  // dialog close.
  DialogChoiceCallback callback = std::move(callback_);
  // Delete class.
  SystemWebDialogDelegate::OnDialogClosed(choice);
  // Run callback after dialog closed.
  if (callback)
    std::move(callback).Run(choice);
}

OfficeFallbackDialog::OfficeFallbackDialog(
    const std::vector<storage::FileSystemURL>& file_urls,
    const std::string& title_text,
    const std::string& reason_message,
    const std::string& instructions_message,
    const int& width,
    const int& height,
    DialogChoiceCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIOfficeFallbackURL),
                              std::u16string() /* title */),
      file_urls_(file_urls),
      title_text_(title_text),
      reason_message_(reason_message),
      instructions_message_(instructions_message),
      width_(width),
      height_(height),
      callback_(std::move(callback)) {}

OfficeFallbackDialog::~OfficeFallbackDialog() = default;

std::string OfficeFallbackDialog::GetDialogArgs() const {
  base::Value::Dict args;
  args.Set("titleText", title_text_);
  args.Set("reasonMessage", reason_message_);
  args.Set("instructionsMessage", instructions_message_);
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void OfficeFallbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(width_, height_);
}

bool OfficeFallbackDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

bool OfficeFallbackDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::office_fallback
