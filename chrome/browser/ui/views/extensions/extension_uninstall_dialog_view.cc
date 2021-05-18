// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr int kCheckboxId = 1;

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
  ui::DialogModel* dialog_model_ = nullptr;

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
  dialog_builder
      .SetTitle(
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
                                     base::UTF8ToUTF16(extension()->name())))
      .OverrideShowCloseButton(false)
      .SetWindowClosingCallback(
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
          l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON))
      .AddCancelButton(
          base::OnceClosure() /* Cancel is covered by WindowClosingCallback */);

  if (triggering_extension()) {
    dialog_builder.AddBodyText(
        ui::DialogModelLabel(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PROMPT_UNINSTALL_TRIGGERED_BY_EXTENSION,
                base::UTF8ToUTF16(triggering_extension()->name())))
            .set_is_secondary()
            .set_allow_character_break());
  }

  if (ShouldShowCheckbox()) {
    dialog_builder.AddCheckbox(kCheckboxId,
                               ui::DialogModelLabel(GetCheckboxLabel()));
  }

  std::unique_ptr<ui::DialogModel> dialog_model = dialog_builder.Build();
  dialog_model_ = dialog_model.get();

  // TODO(devlin): There's a lot of shared-ish code between this and
  // PrintJobConfirmationDialogView. We should pull it into a common location.
  BrowserView* const browser_view =
      parent() ? BrowserView::GetBrowserViewForNativeWindow(parent()) : nullptr;
  ExtensionsToolbarContainer* const container =
      browser_view ? browser_view->toolbar_button_provider()
                         ->GetExtensionsToolbarContainer()
                   : nullptr;
  ToolbarActionView* anchor_view =
      container ? container->GetViewForId(extension()->id()) : nullptr;

  if (anchor_view) {
    DCHECK(container);
    auto bubble = std::make_unique<views::BubbleDialogModelHost>(
        std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);

      container->ShowWidgetForExtension(
          views::BubbleDialogDelegateView::CreateBubble(std::move(bubble)),
          extension()->id());
  } else {
    // TODO(pbos): Add unique_ptr version of CreateBrowserModalDialogViews and
    // remove .release().
    constrained_window::CreateBrowserModalDialogViews(
        views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                  ui::MODAL_TYPE_WINDOW)
            .release(),
        parent())
        ->Show();
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_UNINSTALL);
}

void ExtensionUninstallDialogViews::Close() {
  DCHECK(dialog_model_);
  dialog_model_->host()->Close();
  DCHECK(!dialog_model_);
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
  return CreateViews(profile, parent, delegate);
}

// static
std::unique_ptr<extensions::ExtensionUninstallDialog>
extensions::ExtensionUninstallDialog::CreateViews(Profile* profile,
                                                  gfx::NativeWindow parent,
                                                  Delegate* delegate) {
  return std::make_unique<ExtensionUninstallDialogViews>(profile, parent,
                                                         delegate);
}
