// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class DirectoryAccessConfirmationDialog : public TabModalConfirmDialogDelegate {
 public:
  DirectoryAccessConfirmationDialog(bool writable,
                                    const std::u16string& app_name,
                                    content::WebContents* web_contents,
                                    base::OnceClosure on_accept,
                                    base::OnceClosure on_cancel);

  std::u16string GetTitle() override;
  std::u16string GetDialogMessage() override;
  std::u16string GetAcceptButtonTitle() override;
  std::u16string GetCancelButtonTitle() override;

 private:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  base::OnceClosure on_accept_;
  base::OnceClosure on_cancel_;
  const bool writable_;
  const std::u16string app_name_;
};

DirectoryAccessConfirmationDialog::DirectoryAccessConfirmationDialog(
    bool writable,
    const std::u16string& app_name,
    content::WebContents* web_contents,
    base::OnceClosure on_accept,
    base::OnceClosure on_cancel)
    : TabModalConfirmDialogDelegate(web_contents),
      on_accept_(std::move(on_accept)),
      on_cancel_(std::move(on_cancel)),
      writable_(writable),
      app_name_(app_name) {}

std::u16string DirectoryAccessConfirmationDialog::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_DIRECTORY_CONFIRMATION_DIALOG_TITLE);
}

std::u16string DirectoryAccessConfirmationDialog::GetDialogMessage() {
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

std::u16string DirectoryAccessConfirmationDialog::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
}
std::u16string DirectoryAccessConfirmationDialog::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
}

void DirectoryAccessConfirmationDialog::OnAccepted() {
  std::move(on_accept_).Run();
}

void DirectoryAccessConfirmationDialog::OnCanceled() {
  std::move(on_cancel_).Run();
}

void DirectoryAccessConfirmationDialog::OnClosed() {
  std::move(on_cancel_).Run();
}

}  // namespace

void CreateDirectoryAccessConfirmationDialog(bool writable,
                                             const std::u16string& app_name,
                                             content::WebContents* web_contents,
                                             base::OnceClosure on_accept,
                                             base::OnceClosure on_cancel) {
  TabModalConfirmDialog::Create(
      std::make_unique<DirectoryAccessConfirmationDialog>(
          writable, app_name, web_contents, std::move(on_accept),
          std::move(on_cancel)),
      web_contents);
}
