// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

class PrintJobConfirmationDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit PrintJobConfirmationDialogDelegate(
      base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {}

  void OnDialogAccepted() { std::move(callback_).Run(true); }
  void OnDialogClosed() { std::move(callback_).Run(false); }

 private:
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

namespace extensions {

void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const extensions::ExtensionId& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback) {
  auto bubble_delegate_unique =
      std::make_unique<PrintJobConfirmationDialogDelegate>(std::move(callback));
  PrintJobConfirmationDialogDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_BUBBLE_TITLE))
          .OverrideShowCloseButton(false)
          .SetIcon(ui::ImageModel::FromImageSkia(
              gfx::ImageSkiaOperations::CreateResizedImage(
                  extension_icon,
                  skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
                  gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                            extension_misc::EXTENSION_ICON_SMALL))))
          .AddOkButton(
              base::BindOnce(
                  &PrintJobConfirmationDialogDelegate::OnDialogAccepted,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_ALLOW)))
          .AddCancelButton(
              base::BindOnce(
                  &PrintJobConfirmationDialogDelegate::OnDialogClosed,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_DENY)))
          .AddParagraph(
              ui::DialogModelLabel(
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_BUBBLE_HEADING,
                      extension_name, print_job_title, printer_name))
                  .set_is_secondary()
                  .set_allow_character_break())
          .Build();

  ShowDialog(parent, extension_id, std::move(dialog_model));
}

}  // namespace extensions
