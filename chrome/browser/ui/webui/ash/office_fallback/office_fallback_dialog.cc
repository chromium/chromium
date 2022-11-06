// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "chrome/common/webui_url_constants.h"

namespace {

// Width/height of the Fallback dialog as found with the inspector tool.
const int kWidth = 496;
const int kHeight = 198;

}  // namespace

namespace ash::office_fallback {

// static
bool OfficeFallbackDialog::Show(
    const std::vector<storage::FileSystemURL>& file_urls,
    const FallbackReason fallback_reason,
    const std::u16string& task_title,
    DialogChoiceCallback callback) {
  // Allow no more than one office fallback dialog at a time. In the case of
  // multiple dialog requests, they should either be handled simultaneously or
  // queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUIOfficeFallbackURL))) {
    return false;
  }

  DCHECK(!file_urls.empty());
  if (file_urls.empty()) {
    return false;
  }

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  OfficeFallbackDialog* dialog = new OfficeFallbackDialog(
      file_urls, fallback_reason, task_title, std::move(callback));

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
    const FallbackReason fallback_reason,
    const std::u16string& task_title,
    DialogChoiceCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIOfficeFallbackURL),
                              std::u16string() /* title */),
      file_urls_(file_urls),
      fallback_reason_(fallback_reason),
      task_title_(task_title),
      callback_(std::move(callback)) {}

OfficeFallbackDialog::~OfficeFallbackDialog() = default;

// The mapping should be consistent with
// OfficeFallbackElement.stringToFailureReason in office_fallback_dialog.ts.
std::string FallbackReasonToString(FallbackReason fallback_reason) {
  switch (fallback_reason) {
    case FallbackReason::kOffline:
      return "Offline";
    case FallbackReason::kDriveUnavailable:
      return "Drive Unavailable";
    case FallbackReason::kOneDriveUnavailable:
      return "OneDrive Unavailable";
    case FallbackReason::kErrorOpeningWeb:
      return "Error opening web";
  }
}

std::string OfficeFallbackDialog::GetDialogArgs() const {
  base::Value::Dict args;
  base::Value::List file_names = base::Value::List();
  for (const storage::FileSystemURL& file_url : file_urls_) {
    file_names.Append(base::Value(file_url.path().BaseName().value()));
  }
  args.Set("fileNames", std::move(file_names));
  args.Set("fallbackReason",
           base::Value(FallbackReasonToString(fallback_reason_)));

  args.Set("taskTitle", base::Value(task_title_));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void OfficeFallbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kWidth, kHeight);
}

bool OfficeFallbackDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::office_fallback
