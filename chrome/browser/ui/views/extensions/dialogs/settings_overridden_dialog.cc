// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/dialogs/settings_overridden_dialog.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

using DialogResult = SettingsOverriddenDialogController::DialogResult;

namespace {

// Model delegate that notifies the `controller_` when a click event occurs in
// the settings overriden dialog.
class SettingsOverriddenDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit SettingsOverriddenDialogDelegate(
      std::unique_ptr<SettingsOverriddenDialogController> controller)
      : controller_(std::move(controller)) {}

  void OnDialogAccepted() {
    HandleDialogResult(DialogResult::kChangeSettingsBack);
  }
  void OnDialogCancelled() {
    HandleDialogResult(DialogResult::kKeepNewSettings);
  }
  void OnDialogClosed() { HandleDialogResult(DialogResult::kDialogDismissed); }
  void OnDialogDestroyed() {
    if (!result_) {
      // The dialog may close without firing any of the [accept | cancel |
      // close] callbacks if e.g. the parent window closes. In this case, notify
      // the controller that the dialog closed without user action.
      HandleDialogResult(DialogResult::kDialogClosedWithoutUserAction);
    }
  }

  SettingsOverriddenDialogController* controller() { return controller_.get(); }

 private:
  void HandleDialogResult(DialogResult result) {
    DCHECK(!result_)
        << "Trying to re-notify controller of result. Previous result: "
        << static_cast<int>(*result_)
        << ", new result: " << static_cast<int>(result);
    result_ = result;
    controller_->HandleDialogResult(result);
  }
  std::unique_ptr<SettingsOverriddenDialogController> controller_;
  std::optional<DialogResult> result_;
};

}  // namespace

namespace extensions {

void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    Browser* browser) {
  SettingsOverriddenDialogController::ShowParams show_params =
      controller->GetShowParams();

  auto dialog_delegate_unique =
      std::make_unique<SettingsOverriddenDialogDelegate>(std::move(controller));
  SettingsOverriddenDialogDelegate* dialog_delegate =
      dialog_delegate_unique.get();

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique));
  dialog_builder.SetInternalName(kExtensionSettingsOverriddenDialogName)
      .SetTitle(show_params.dialog_title)
      .AddParagraph(ui::DialogModelLabel(show_params.message))
      .AddOkButton(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogAccepted,
                         base::Unretained(dialog_delegate)),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_CHANGE_IT_BACK)))
      .AddCancelButton(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogCancelled,
                         base::Unretained(dialog_delegate)),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_KEEP_IT)))
      .SetCloseActionCallback(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogClosed,
                         base::Unretained(dialog_delegate)))
      .SetDialogDestroyingCallback(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogDestroyed,
                         base::Unretained(dialog_delegate)))
      .OverrideShowCloseButton(false);

  if (show_params.icon) {
    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(*show_params.icon,
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE),
                              ui::kColorIcon);

    dialog_builder.SetIcon(ui::ImageModel::FromImageSkia(icon));
  }

  constrained_window::ShowBrowserModal(dialog_builder.Build(),
                                       browser->window()->GetNativeWindow());
  dialog_delegate->controller()->OnDialogShown();
}

}  // namespace extensions
