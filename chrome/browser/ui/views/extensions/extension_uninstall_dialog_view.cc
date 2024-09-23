// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/view.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCheckboxId);

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews
    : public extensions::ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(
      Profile* profile,
      gfx::NativeWindow parent,
      extensions::ExtensionUninstallDialog::Delegate* delegate);
  ExtensionUninstallDialogViews(const ExtensionUninstallDialogViews&) = delete;
  ExtensionUninstallDialogViews& operator=(
      const ExtensionUninstallDialogViews&) = delete;
  ~ExtensionUninstallDialogViews() override;

  // Forwards that the dialog has been accepted to the delegate.
  void DialogAccepted();
  // Reports a canceled dialog to the delegate (unless accepted).
  void DialogClosing();

 private:
  void Show() override;
  void Close() override;

  // Pointer to the DialogModel for the dialog. This is cleared when the dialog
  // is being closed and OnDialogClosed is reported. As such it prevents access
  // to the dialog after it's been closed, as well as preventing multiple
  // reports of OnDialogClosed.
  raw_ptr<ui::DialogModel> dialog_model_ = nullptr;

  // WeakPtrs because the associated dialog may outlive |this|, which is owned
  // by the caller of extensions::ExtensionsUninstallDialog::Create().
  base::WeakPtrFactory<ExtensionUninstallDialogViews> weak_ptr_factory_{this};
};

ExtensionUninstallDialogViews::ExtensionUninstallDialogViews(
    Profile* profile,
    gfx::NativeWindow parent,
    extensions::ExtensionUninstallDialog::Delegate* delegate)
    : extensions::ExtensionUninstallDialog(profile, parent, delegate) {}

ExtensionUninstallDialogViews::~ExtensionUninstallDialogViews() {
  if (dialog_model_)
    dialog_model_->host()->Close();
  DCHECK(!dialog_model_);
}

void ExtensionUninstallDialogViews::Show() {
  // TODO(pbos): Consider separating dialog model from views code.
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetInternalName("ExtensionUninstallDialog")
      .SetTitle(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
          extensions::util::GetFixupExtensionNameForUIDisplay(
              extension()->name())))
      .OverrideShowCloseButton(false)
      .SetDialogDestroyingCallback(
          base::BindOnce(&ExtensionUninstallDialogViews::DialogClosing,
                         weak_ptr_factory_.GetWeakPtr()))
      .SetIcon(ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateResizedImage(
              icon(), skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
              gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                        extension_misc::EXTENSION_ICON_SMALL))))
      .AddOkButton(
          base::BindOnce(&ExtensionUninstallDialogViews::DialogAccepted,
                         weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON)))
      .AddCancelButton(
          base::DoNothing() /* Cancel is covered by WindowClosingCallback */);

  if (triggering_extension()) {
    dialog_builder.AddParagraph(
        ui::DialogModelLabel(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PROMPT_UNINSTALL_TRIGGERED_BY_EXTENSION,
                extensions::util::GetFixupExtensionNameForUIDisplay(
                    triggering_extension()->name())))
            .set_is_secondary()
            .set_allow_character_break());
  }

  if (ShouldShowCheckbox()) {
    std::u16string checkbox_label =
        triggering_extension()
            ? l10n_util::GetStringFUTF16(
                  IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE_FROM_EXTENSION,
                  extensions::util::GetFixupExtensionNameForUIDisplay(
                      extension()->name()))
            : l10n_util::GetStringUTF16(
                  IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE);

    dialog_builder.AddCheckbox(kCheckboxId,
                               ui::DialogModelLabel(checkbox_label));
  }

  std::unique_ptr<ui::DialogModel> dialog_model = dialog_builder.Build();
  dialog_model_ = dialog_model.get();

  ShowDialog(parent(), extension()->id(), std::move(dialog_model));
}

void ExtensionUninstallDialogViews::Close() {
  DCHECK(dialog_model_);
  dialog_model_->host()->Close();
}

void ExtensionUninstallDialogViews::DialogAccepted() {
  DCHECK(dialog_model_);
  const bool checkbox_is_checked =
      ShouldShowCheckbox() &&
      dialog_model_->GetCheckboxByUniqueId(kCheckboxId)->is_checked();
  dialog_model_ = nullptr;
  OnDialogClosed(checkbox_is_checked
                     ? CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED
                     : CLOSE_ACTION_UNINSTALL);
}

void ExtensionUninstallDialogViews::DialogClosing() {
  if (!dialog_model_)
    return;
  dialog_model_ = nullptr;
  OnDialogClosed(CLOSE_ACTION_CANCELED);
}

}  // namespace

// static
std::unique_ptr<extensions::ExtensionUninstallDialog>
extensions::ExtensionUninstallDialog::Create(Profile* profile,
                                             gfx::NativeWindow parent,
                                             Delegate* delegate) {
  return std::make_unique<ExtensionUninstallDialogViews>(profile, parent,
                                                         delegate);
}
