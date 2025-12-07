// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace {

// Model delegate that notifies the `controller_` when a click event occurs in
// the controlled home dialog.
class ControlledHomeDialogDelegate
    : public ui::DialogModelDelegate,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit ControlledHomeDialogDelegate(
      Profile* profile,
      std::unique_ptr<ControlledHomeDialogControllerInterface> controller)
      : controller_(std::move(controller)) {
    extension_registry_observation_.Observe(
        extensions::ExtensionRegistry::Get(profile));
  }

  void OnDialogAccepted() {
    controller_->OnBubbleClosed(
        ControlledHomeDialogControllerInterface::CloseAction::CLOSE_EXECUTE);
  }
  void OnDialogCancelled() {
    controller_->OnBubbleClosed(ControlledHomeDialogControllerInterface::
                                    CloseAction::CLOSE_DISMISS_USER_ACTION);
  }
  void OnLearnMoreClicked() {
    controller_->OnBubbleClosed(
        ControlledHomeDialogControllerInterface::CloseAction::CLOSE_LEARN_MORE);
  }
  void OnDialogClosed() {
    controller_->OnBubbleClosed(ControlledHomeDialogControllerInterface::
                                    CloseAction::CLOSE_DISMISS_DEACTIVATION);
  }

  ControlledHomeDialogControllerInterface* controller() {
    return controller_.get();
  }

 private:
  void CloseDialog() { dialog_model()->host()->Close(); }

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (extension->id() != controller_->GetAnchorActionId()) {
      return;
    }

    CloseDialog();
  }
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    if (extension->id() != controller_->GetAnchorActionId()) {
      return;
    }

    CloseDialog();
  }
  void OnShutdown(extensions::ExtensionRegistry* registry) override {
    // It is possible that the extension registry is destroyed before the
    // dialog. In such case, the controller should no longer observe the
    // registry.
    DCHECK(extension_registry_observation_.IsObservingSource(registry));
    extension_registry_observation_.Reset();
  }

  std::unique_ptr<ControlledHomeDialogControllerInterface> controller_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kControlledHomeDialogCancelButtonElementId);

void ShowControlledHomeDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    std::unique_ptr<ControlledHomeDialogControllerInterface>
        dialog_controller_unique) {
  auto dialog_delegate_unique = std::make_unique<ControlledHomeDialogDelegate>(
      profile, std::move(dialog_controller_unique));
  ControlledHomeDialogDelegate* dialog_delegate = dialog_delegate_unique.get();
  ControlledHomeDialogControllerInterface* dialog_controller =
      dialog_delegate->controller();

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique));
  dialog_builder.SetTitle(dialog_controller->GetHeadingText())
      .AddParagraph(ui::DialogModelLabel(dialog_controller->GetBodyText()))
      .AddCancelButton(
          base::BindOnce(&ControlledHomeDialogDelegate::OnDialogCancelled,
                         base::Unretained(dialog_delegate)),
          ui::DialogModel::Button::Params()
              .SetLabel(dialog_controller->GetDismissButtonText())
              .SetId(kControlledHomeDialogCancelButtonElementId))
      .SetCloseActionCallback(
          base::BindOnce(&ControlledHomeDialogDelegate::OnDialogClosed,
                         base::Unretained(dialog_delegate)));

  if (dialog_controller->IsPolicyIndicationNeeded()) {
    dialog_builder.AddMenuItem(
        ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                       ui::kColorIcon, 16),
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
        base::DoNothing(),
        ui::DialogModelMenuItem::Params().SetIsEnabled(false));

  } else {
    dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
        IDS_EXTENSIONS_CONTROLLED_HOME_DIALOG_LEARN_MORE_LINK,
        ui::DialogModelLabel::CreateLink(
            IDS_LEARN_MORE,
            base::BindRepeating(
                &ControlledHomeDialogDelegate::OnLearnMoreClicked,
                base::Unretained(dialog_delegate)))));
  }

  std::u16string ok_button_text = dialog_controller->GetActionButtonText();
  if (!ok_button_text.empty()) {
    dialog_builder.AddOkButton(
        base::BindOnce(&ControlledHomeDialogDelegate::OnDialogAccepted,
                       base::Unretained(dialog_delegate)),
        ui::DialogModel::Button::Params().SetLabel(ok_button_text));
  }

  ShowDialog(parent, dialog_controller->GetAnchorActionId(),
             dialog_builder.Build());
  dialog_controller->OnBubbleShown();
}

}  // namespace extensions
