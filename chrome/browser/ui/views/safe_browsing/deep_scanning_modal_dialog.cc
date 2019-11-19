// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/deep_scanning_modal_dialog.h"

#include "base/callback_forward.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

DeepScanningModalDialog::DeepScanningModalDialog(
    content::WebContents* web_contents,
    base::OnceClosure accept_callback)
    : TabModalConfirmDialogDelegate(web_contents),
      accept_callback_(std::move(accept_callback)) {}

DeepScanningModalDialog::~DeepScanningModalDialog() {}

base::string16 DeepScanningModalDialog::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_TITLE);
}

base::string16 DeepScanningModalDialog::GetDialogMessage() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_MESSAGE);
}

base::string16 DeepScanningModalDialog::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_ACCEPT_BUTTON);
}

base::string16 DeepScanningModalDialog::GetLinkText() const {
  // TODO(drubery): Once we have an FAQ page for download deep scanning, link it
  // here.
  return base::string16();
}

int DeepScanningModalDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

void DeepScanningModalDialog::OnLinkClicked(WindowOpenDisposition disposition) {
  // TODO(drubery): Once we have an FAQ page for download deep scanning, link it
  // here;
}

void DeepScanningModalDialog::OnAccepted() {
  std::move(accept_callback_).Run();
}

}  // namespace safe_browsing
