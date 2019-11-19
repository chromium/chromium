// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class DirectoryAccessConfirmationDialog : public TabModalConfirmDialogDelegate {
 public:
  DirectoryAccessConfirmationDialog(bool writable,
                                    const base::string16& app_name,
                                    content::WebContents* web_contents,
                                    const base::Closure& on_accept,
                                    const base::Closure& on_cancel);

  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  base::string16 GetAcceptButtonTitle() override;
  base::string16 GetCancelButtonTitle() override;

 private:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  const base::Closure on_accept_;
  const base::Closure on_cancel_;
  const bool writable_;
  const base::string16 app_name_;
};

DirectoryAccessConfirmationDialog::DirectoryAccessConfirmationDialog(
    bool writable,
    const base::string16& app_name,
    content::WebContents* web_contents,
    const base::Closure& on_accept,
    const base::Closure& on_cancel)
    : TabModalConfirmDialogDelegate(web_contents),
      on_accept_(on_accept),
      on_cancel_(on_cancel),
      writable_(writable),
      app_name_(app_name) {}

base::string16 DirectoryAccessConfirmationDialog::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_DIRECTORY_CONFIRMATION_DIALOG_TITLE);
}

base::string16 DirectoryAccessConfirmationDialog::GetDialogMessage() {
  if (writable_) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_DIRECTORY_CONFIRMATION_DIALOG_MESSAGE_WRITABLE,
        app_name_);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_DIRECTORY_CONFIRMATION_DIALOG_MESSAGE_READ_ONLY,
        app_name_);
  }
}

base::string16 DirectoryAccessConfirmationDialog::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
}
base::string16 DirectoryAccessConfirmationDialog::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
}

void DirectoryAccessConfirmationDialog::OnAccepted() {
  on_accept_.Run();
}

void DirectoryAccessConfirmationDialog::OnCanceled() {
  on_cancel_.Run();
}

void DirectoryAccessConfirmationDialog::OnClosed() {
  on_cancel_.Run();
}

}  // namespace

void CreateDirectoryAccessConfirmationDialog(bool writable,
                                             const base::string16& app_name,
                                             content::WebContents* web_contents,
                                             const base::Closure& on_accept,
                                             const base::Closure& on_cancel) {
  TabModalConfirmDialog::Create(
      std::make_unique<DirectoryAccessConfirmationDialog>(
          writable, app_name, web_contents, on_accept, on_cancel),
      web_contents);
}
