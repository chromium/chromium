// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/deep_scanning_modal_dialog.h"

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

std::u16string DeepScanningModalDialog::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_TITLE);
}

std::u16string DeepScanningModalDialog::GetDialogMessage() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_MESSAGE);
}

std::u16string DeepScanningModalDialog::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_OPEN_NOW_ACCEPT_BUTTON);
}

std::u16string DeepScanningModalDialog::GetLinkText() const {
  // TODO(drubery): Once we have an FAQ page for download deep scanning, link it
  // here.
  return std::u16string();
}

void DeepScanningModalDialog::OnLinkClicked(WindowOpenDisposition disposition) {
  // TODO(drubery): Once we have an FAQ page for download deep scanning, link it
  // here;
}

void DeepScanningModalDialog::OnAccepted() {
  std::move(accept_callback_).Run();
}

}  // namespace safe_browsing
