// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace {

class DownloadOpenConfirmationDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit DownloadOpenConfirmationDialogDelegate(
      base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {}

  ~DownloadOpenConfirmationDialogDelegate() override = default;

  void OnDialogAccepted() { std::move(callback_).Run(true); }
  void OnDialogCanceled() { std::move(callback_).Run(false); }
  void OnDialogDestroyed() {
    if (callback_) {
      std::move(callback_).Run(false);
    }
  }

 private:
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

namespace extensions {

void ShowDownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> open_callback) {
  auto dialog_delegate_unique =
      std::make_unique<DownloadOpenConfirmationDialogDelegate>(
          std::move(open_callback));
  DownloadOpenConfirmationDialogDelegate* dialog_delegate =
      dialog_delegate_unique.get();

  std::unique_ptr<ui::DialogModel> dialog =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_OPEN_CONFIRMATION_DIALOG_TITLE))
          .AddOkButton(
              base::BindOnce(
                  &DownloadOpenConfirmationDialogDelegate::OnDialogAccepted,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)))
          .AddCancelButton(
              base::BindOnce(
                  &DownloadOpenConfirmationDialogDelegate::OnDialogCanceled,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL)))
          .SetCloseActionCallback(base::BindOnce(
              &DownloadOpenConfirmationDialogDelegate::OnDialogCanceled,
              base::Unretained(dialog_delegate)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &DownloadOpenConfirmationDialogDelegate::OnDialogDestroyed,
              base::Unretained(dialog_delegate)))
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
              IDS_DOWNLOAD_OPEN_CONFIRMATION_DIALOG_MESSAGE,
              {ui::DialogModelLabel::CreatePlainText(
                   extensions::util::GetFixupExtensionNameForUIDisplay(
                       extension_name)),
               ui::DialogModelLabel::CreatePlainText(
                   file_path.BaseName().AsUTF16Unsafe())}))
          .Build();

  ShowWebModalDialog(web_contents, std::move(dialog));
}

}  // namespace extensions
