// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/os_feedback_dialog.h"

#include "ash/webui/os_feedback_ui/url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace {

// All Feedback Tool window will be a fixed 600px*640dp portal per
// specification.
constexpr int kDialogWidth = 600;
constexpr int kDialogHeight = 640;

}  // namespace

namespace ash {

void OsFeedbackDialog::ShowDialog(
    content::BrowserContext* context,
    const extensions::api::feedback_private::FeedbackInfo& info,
    gfx::NativeWindow parent) {
  auto* dialog = new OsFeedbackDialog(info);
  dialog->ShowSystemDialogForBrowserContext(context, parent);
}

// Protected.
OsFeedbackDialog::OsFeedbackDialog(
    const extensions::api::feedback_private::FeedbackInfo& info)
    : SystemWebDialogDelegate(GURL(kChromeUIOSFeedbackUrl),
                              /* title=*/std::u16string()) {}

OsFeedbackDialog::~OsFeedbackDialog() = default;

// Private.
void OsFeedbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

}  // namespace ash
