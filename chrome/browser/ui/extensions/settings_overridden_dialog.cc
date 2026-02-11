// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_dialog.h"

#include <memory>
#include <optional>
#include <utility>

#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view_utils.h"  // nogncheck
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using DialogResult = SettingsOverriddenDialogController::DialogResult;

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kSettingsOverriddenDialogPreviousSettingButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogNewSettingButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogSaveButtonId);

// Model delegate that notifies the `controller_` when a click event occurs in
// the settings overridden dialog.
class SettingsOverriddenDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit SettingsOverriddenDialogDelegate(
      std::unique_ptr<SettingsOverriddenDialogController> controller)
      : controller_(std::move(controller)) {}

  static base::PassKey<SettingsOverriddenDialogDelegate> GetPassKey() {
    return {};
  }

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

  // When the dialog is showing explicit choices, is called when the selected
  // option is initially set or changed (ie. when the user selects one of the
  // radio buttons).
  void OnRadioSelectionChanged(DialogResult result) {
    selected_setting_ = result;
    auto* model = dialog_model();
    model->SetButtonEnabled(
        model->GetButtonByUniqueId(kSettingsOverriddenDialogSaveButtonId),
        true);
  }

  // Called when the "Save" button on an explicit-choice dialog is clicked, to
  // commit the currently-selected radio button's option.
  void OnDialogSelectionSaved() {
    CHECK(selected_setting_);
    HandleDialogResult(selected_setting_.value());
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

  // When offering an explicit choice, stores the currently selected option,
  // which will be locked in when the dialog's Save button is clicked.
  std::optional<DialogResult> selected_setting_;
};

namespace {
constexpr int kDialogHeaderIconSize = 20;

void BuildSettingsOverriddenDialog(
    ui::DialogModel::Builder& dialog_builder,
    const SettingsOverriddenDialogController::ShowParams& show_params,
    SettingsOverriddenDialogDelegate* dialog_delegate) {
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
    dialog_builder.SetIcon(ui::ImageModel::FromVectorIcon(
        // TODO(crbug.com/439918265): Align on a single icon size for extension
        // dialogs and use such variable here.
        *show_params.icon, ui::kColorIcon, kDialogHeaderIconSize));
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void BuildExplicitChoiceDialog(
    ui::DialogModel::Builder& dialog_builder,
    const SettingsOverriddenDialogController::ShowParams& show_params,
    SettingsOverriddenDialogDelegate* dialog_delegate) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kSearchEngineExplicitChoiceDialog));
  dialog_builder.SetElementIdentifier(kSettingsOverriddenDialogId)
      .SetTitle(show_params.dialog_title)
      .AddParagraph(ui::DialogModelLabel(show_params.message), std::u16string())
      .AddOkButton(
          base::BindOnce(
              &SettingsOverriddenDialogDelegate::OnDialogSelectionSaved,
              base::Unretained(dialog_delegate)),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(IDS_SAVE))
              .SetEnabled(false)
              .SetId(kSettingsOverriddenDialogSaveButtonId))
      .SetCloseActionCallback(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogClosed,
                         base::Unretained(dialog_delegate)))
      .SetDialogDestroyingCallback(
          base::BindOnce(&SettingsOverriddenDialogDelegate::OnDialogDestroyed,
                         base::Unretained(dialog_delegate)))
      .SetInitiallyFocusedField(kSettingsOverriddenDialogSaveButtonId)
      .OverrideShowCloseButton(false)
      .DisableCloseOnEscape(SettingsOverriddenDialogDelegate::GetPassKey());

  // Helper to bind the selection callback to a specific result.
  auto create_selection_callback = [&](DialogResult result) {
    return base::BindRepeating(
        &SettingsOverriddenDialogDelegate::OnRadioSelectionChanged,
        base::Unretained(dialog_delegate), result);
  };

  extensions::AddExplicitChoiceRadioButtons(
      dialog_builder, show_params.previous_setting.value(),
      kSettingsOverriddenDialogPreviousSettingButtonId,
      create_selection_callback(DialogResult::kChangeSettingsBack),
      show_params.new_setting.value(),
      kSettingsOverriddenDialogNewSettingButtonId,
      create_selection_callback(DialogResult::kKeepNewSettings));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

namespace extensions {

void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    gfx::NativeWindow parent) {
  SettingsOverriddenDialogController::ShowParams show_params =
      controller->GetShowParams();

  auto dialog_delegate_unique =
      std::make_unique<SettingsOverriddenDialogDelegate>(std::move(controller));
  SettingsOverriddenDialogDelegate* dialog_delegate =
      dialog_delegate_unique.get();

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique));
  dialog_builder.SetInternalName(kExtensionSettingsOverriddenDialogName);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // The "explicit choice" dialog is only supported on Windows and Mac.
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog) &&
      show_params.new_setting && show_params.previous_setting) {
    // Explicit choice options are present, so show the corresponding dialog.
    BuildExplicitChoiceDialog(dialog_builder, show_params, dialog_delegate);
  } else {
    BuildSettingsOverriddenDialog(dialog_builder, show_params, dialog_delegate);
  }
#else   // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  BuildSettingsOverriddenDialog(dialog_builder, show_params, dialog_delegate);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  ShowModalDialog(parent, dialog_builder.Build());

  dialog_delegate->controller()->OnDialogShown();
}

}  // namespace extensions
