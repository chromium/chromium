// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/os_feedback_dialog.h"

#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace {

// All Feedback Tool window will be a fixed 600px*640dp portal per
// specification.
constexpr int kDialogWidth = 600;
constexpr int kDialogHeight = 640;

GURL GetUrl() {
  return GURL{ash::kChromeUIOSFeedbackUrl};
}

}  // namespace

namespace ash {

void OsFeedbackDialog::ShowDialog(
    content::BrowserContext* context,
    const extensions::api::feedback_private::FeedbackInfo& info,
    gfx::NativeWindow parent) {
  // If a dialog is opened, focus on it.
  auto* existing_instance =
      SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (existing_instance) {
    existing_instance->Focus();
    return;
  }

  auto* dialog = new OsFeedbackDialog(info);
  dialog->ShowSystemDialogForBrowserContext(context, parent);
}

// Protected.
OsFeedbackDialog::OsFeedbackDialog(
    const extensions::api::feedback_private::FeedbackInfo& info)
    : SystemWebDialogDelegate(GetUrl(),
                              /* title=*/std::u16string()),
      feedback_info_(info.ToValue()) {}

OsFeedbackDialog::~OsFeedbackDialog() = default;

// Private.
void OsFeedbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

std::string OsFeedbackDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(feedback_info_, &data);
  return data;
}

}  // namespace ash
