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
// A "-1" resource ID is used to indicate that the fallback dialog will not
// display a "fallback reason" message.
const int kNoReasonMessageResourceId = -1;

// Width of the Fallback dialog as found with the inspector tool.
const int kWidth = 512;

// Exact height of the Fallback dialogs required for different texts (in
// English) as found with the inspector tool.
const int kOfflineHeight = 244;
const int kDisableDrivePreferenceSetHeight = 268;
const int kDriveUnavailableHeight = 268;
const int kDriveDisabledForAccountType = 268;
const int kMeteredHeight = 268;
const int kWaitingForUploadHeight = 228;
const int kAndroidOneDriveUnsupportedLocationHeight = 244;

// Height of a line of text as found with the inspector tool.
const int kLineHeight = 20;

// Get the text ids for the `fallback_reason` specific translated strings that
// will be displayed in dialog. Store them in the out parameters `title_id`,
// `reason_message_id` and `instructions_message_id`. Get the corresponding
// width and height needed to display these strings in the dialog. Store them in
// the out parameters `width` and `height`.
void GetDialogTextIdsAndSize(
    const ash::office_fallback::FallbackReason fallback_reason,
    int& title_id,
    int& reason_message_id,
    bool& include_task_in_reason_message,
    int& instructions_message_id,
    bool& enable_retry_option,
    bool& enable_quick_office_option,
    int& width,
    int& height) {
  width = kWidth;
  include_task_in_reason_message = false;
  switch (fallback_reason) {
    case ash::office_fallback::FallbackReason::kOffline:
    case ash::office_fallback::FallbackReason::kDriveAuthenticationNotReady:
      title_id = IDS_OFFICE_FALLBACK_TITLE_OFFLINE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_OFFLINE;
      include_task_in_reason_message = true;
      instructions_message_id = IDS_OFFICE_FALLBACK_INSTRUCTIONS_OFFLINE;
      enable_retry_option = true;
      enable_quick_office_option = true;
      height = kOfflineHeight;
      break;
    case ash::office_fallback::FallbackReason::kDisableDrivePreferenceSet:
      title_id = IDS_OFFICE_FALLBACK_TITLE_DRIVE_UNAVAILABLE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_DRIVE_UNAVAILABLE;
      include_task_in_reason_message = true;
      instructions_message_id =
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_DISABLE_DRIVE_PREFERENCE;
      enable_retry_option = true;
      enable_quick_office_option = true;
      height = kDisableDrivePreferenceSetHeight;
      break;
    case ash::office_fallback::FallbackReason::kDriveDisabledForAccountType:
      title_id = IDS_OFFICE_FALLBACK_TITLE_DRIVE_UNAVAILABLE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_DRIVE_DISABLED_FOR_ACCOUNT;
      instructions_message_id =
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_DRIVE_DISABLED_FOR_ACCOUNT;
      enable_retry_option = true;
      enable_quick_office_option = true;
      height = kDriveDisabledForAccountType;
      break;
    case ash::office_fallback::FallbackReason::kMeteredConnection:
      title_id = IDS_OFFICE_FALLBACK_TITLE_METERED;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_METERED;
      instructions_message_id = IDS_OFFICE_FALLBACK_INSTRUCTIONS_METERED;
      enable_retry_option = true;
      enable_quick_office_option = true;
      height = kMeteredHeight;
      break;
    case ash::office_fallback::FallbackReason::kDriveDisabled:
    case ash::office_fallback::FallbackReason::kNoDriveService:
    case ash::office_fallback::FallbackReason::kDriveFsInterfaceError:
      title_id = IDS_OFFICE_FALLBACK_TITLE_DRIVE_UNAVAILABLE;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_DRIVE_UNAVAILABLE;
      include_task_in_reason_message = true;
      instructions_message_id = IDS_OFFICE_FALLBACK_INSTRUCTIONS;
      enable_retry_option = true;
      enable_quick_office_option = true;
      height = kDriveUnavailableHeight;
      break;
    case ash::office_fallback::FallbackReason::kWaitingForUpload:
      title_id = IDS_OFFICE_FALLBACK_TITLE_WAITING_FOR_UPLOAD;
      reason_message_id = IDS_OFFICE_FALLBACK_REASON_WAITING_FOR_UPLOAD;
      instructions_message_id =
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_WAITING_FOR_UPLOAD;
      enable_retry_option = false;
      enable_quick_office_option = false;
      height = kWaitingForUploadHeight;
      break;
    case ash::office_fallback::FallbackReason::
        kAndroidOneDriveUnsupportedLocation:
      title_id =
          IDS_OFFICE_FALLBACK_TITLE_ANDROID_ONE_DRIVE_LOCATION_NOT_SUPPORTED;
      reason_message_id = kNoReasonMessageResourceId;
      instructions_message_id =
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_ANDROID_ONE_DRIVE_LOCATION_NOT_SUPPORTED;
      enable_retry_option = false;
      enable_quick_office_option = true;
      height = kAndroidOneDriveUnsupportedLocationHeight;
      break;
  }
  // Add extra height to account for translations.
  height += kLineHeight;
}
}  // namespace

namespace ash::office_fallback {

// static
bool OfficeFallbackDialog::Show(
    const std::vector<storage::FileSystemURL>& file_urls,
    FallbackReason fallback_reason,
    const std::string& task_title,
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

  if (task_title.empty()) {
    LOG(WARNING) << "task_title was empty";
    std::move(callback).Run(std::nullopt);
    return false;
  }

  // Get failure specific text to display in dialog.
  int title_id;
  int reason_message_id;
  bool include_task_in_reason_message;
  int instructions_message_id;
  bool enable_retry_option;
  bool enable_quick_office_option;
  int width;
  int height;
  GetDialogTextIdsAndSize(fallback_reason, title_id, reason_message_id,
                          include_task_in_reason_message,
                          instructions_message_id, enable_retry_option,
                          enable_quick_office_option, width, height);
  // TODO(cassycc): Figure out how to add the web_drive to the placeholder in
  // IDS_OFFICE_FALLBACK_TITLE_WEB_DRIVE_UNAVAILABLE.
  const std::string title_text = l10n_util::GetStringFUTF8(title_id, file_name);
  std::string reason_message = "";
  if (reason_message_id != kNoReasonMessageResourceId) {
    reason_message = include_task_in_reason_message
                         ? l10n_util::GetStringFUTF8(
                               reason_message_id, base::UTF8ToUTF16(task_title))
                         : l10n_util::GetStringUTF8(reason_message_id);
  }
  const std::string instructions_message =
      l10n_util::GetStringUTF8(instructions_message_id);

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  OfficeFallbackDialog* dialog = new OfficeFallbackDialog(
      file_urls, title_text, reason_message, instructions_message,
      enable_retry_option, enable_quick_office_option, width, height,
      std::move(callback));

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
    const bool& enable_retry_option,
    const bool& enable_quick_office_option,
    const int& width,
    const int& height,
    DialogChoiceCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIOfficeFallbackURL),
                              std::u16string() /* title */),
      file_urls_(file_urls),
      title_text_(title_text),
      reason_message_(reason_message),
      instructions_message_(instructions_message),
      enable_retry_option_(enable_retry_option),
      enable_quick_office_option_(enable_quick_office_option),
      width_(width),
      height_(height),
      callback_(std::move(callback)) {}

OfficeFallbackDialog::~OfficeFallbackDialog() = default;

std::string OfficeFallbackDialog::GetDialogArgs() const {
  base::Value::Dict args;
  args.Set("titleText", title_text_);
  args.Set("reasonMessage", reason_message_);
  args.Set("instructionsMessage", instructions_message_);
  args.Set("enableRetryOption", enable_retry_option_);
  args.Set("enableQuickOfficeOption", enable_quick_office_option_);
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
