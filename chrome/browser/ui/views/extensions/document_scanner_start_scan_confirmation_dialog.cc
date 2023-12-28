// Copyright 2023 The Chromium Authors
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

class DocumentScannerStartScanConfirmationDialogDelegate
    : public ui::DialogModelDelegate {
 public:
  explicit DocumentScannerStartScanConfirmationDialogDelegate(
      base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {}

  void OnDialogAccepted() { std::move(callback_).Run(true); }
  void OnDialogClosed() { std::move(callback_).Run(false); }

 private:
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

namespace extensions {

void ShowDocumentScannerStartScanConfirmationDialog(
    gfx::NativeWindow parent,
    const extensions::ExtensionId& extension_id,
    const std::u16string& extension_name,
    const std::u16string& scanner_name,
    const gfx::ImageSkia& extension_icon,
    base::OnceCallback<void(bool)> callback) {
  auto bubble_delegate_unique =
      std::make_unique<DocumentScannerStartScanConfirmationDialogDelegate>(
          std::move(callback));
  DocumentScannerStartScanConfirmationDialogDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_DOCUMENT_SCAN_API_START_SCAN_REQUEST_BUBBLE_TITLE))
          .OverrideShowCloseButton(false)
          .SetIcon(ui::ImageModel::FromImageSkia(
              gfx::ImageSkiaOperations::CreateResizedImage(
                  extension_icon,
                  skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
                  gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                            extension_misc::EXTENSION_ICON_SMALL))))
          .AddOkButton(
              base::BindOnce(
                  &DocumentScannerStartScanConfirmationDialogDelegate::
                      OnDialogAccepted,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_DOCUMENT_SCAN_API_START_SCAN_REQUEST_ALLOW)))
          .AddCancelButton(
              base::BindOnce(
                  &DocumentScannerStartScanConfirmationDialogDelegate::
                      OnDialogClosed,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_DOCUMENT_SCAN_API_START_SCAN_REQUEST_DENY)))
          .AddParagraph(
              ui::DialogModelLabel(
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSIONS_DOCUMENT_SCAN_API_START_SCAN_REQUEST_BUBBLE_HEADING,
                      extension_name, scanner_name))
                  .set_is_secondary()
                  .set_allow_character_break())
          .SetCloseActionCallback(base::BindOnce(
              &DocumentScannerStartScanConfirmationDialogDelegate::
                  OnDialogClosed,
              base::Unretained(bubble_delegate)))
          .Build();

  ShowDialog(parent, extension_id, std::move(dialog_model));
}

}  // namespace extensions
