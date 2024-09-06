// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"

namespace extensions {

void ShowMv2DeprecationDisabledDialog(
    Browser* browser,
    std::vector<Mv2DisabledDialogController::ExtensionInfo>& extensions_info,
    base::OnceClosure remove_callback,
    base::OnceClosure manage_callback,
    base::OnceClosure close_callback) {
  CHECK(!extensions_info.empty());
  int extensions_size = extensions_info.size();

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetInternalName("Mv2DeprecationDisabledDialog")
      .AddParagraph(
          ui::DialogModelLabel(l10n_util::GetPluralStringFUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_DESCRIPTION,
              extensions_size)),
          /*header=*/std::u16string(),
          /*id=*/kExtensionsMv2DisabledDialogParagraphElementId)
      .AddOkButton(
          std::move(remove_callback),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_OK_BUTTON))
              .SetId(kExtensionsMv2DisabledDialogRemoveButtonElementId))
      .AddCancelButton(
          std::move(manage_callback),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetPluralStringFUTF16(
                  IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_CANCEL_BUTTON,
                  extensions_size))
              .SetId(kExtensionsMv2DisabledDialogManageButtonElementId))
      .DisableCloseOnDeactivate()
      .SetCloseActionCallback(std::move(close_callback));

  if (extensions_size == 1) {
    dialog_builder
        .SetTitle(l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_TITLE,
            base::UTF8ToUTF16(extensions_info[0].name)))
        .SetIcon(ui::ImageModel::FromImage(extensions_info[0].icon));

  } else {
    dialog_builder.SetTitle(l10n_util::GetStringFUTF16Int(
        IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_PLURAL_TITLE,
        extensions_size));
    for (const auto& extension_info : extensions_info) {
      dialog_builder.AddMenuItem(
          ui::ImageModel::FromImage(extension_info.icon),
          base::UTF8ToUTF16(extension_info.name), base::DoNothing(),
          ui::DialogModelMenuItem::Params().SetIsEnabled(false));
    }
  }

  ShowDialog(browser, dialog_builder.Build());
}

}  // namespace extensions
