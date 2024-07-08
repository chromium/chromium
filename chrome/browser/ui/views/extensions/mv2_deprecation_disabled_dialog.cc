// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace extensions {

void ShowMv2DeprecationDisabledDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    const std::vector<ExtensionId>& extension_ids,
    base::OnceClosure remove_callback,
    base::OnceClosure manage_callback) {
  CHECK(!extension_ids.empty());
  int extensions_size = extension_ids.size();

  ui::DialogModel::Builder dialog_builder;
  dialog_builder
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetPluralStringFUTF16(
          IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_DESCRIPTION,
          extensions_size)))
      .AddOkButton(
          std::move(remove_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_OK_BUTTON)))
      .AddCancelButton(
          std::move(manage_callback),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetPluralStringFUTF16(
                  IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_CANCEL_BUTTON,
                  extensions_size)));

  auto* extension_registry = ExtensionRegistry::Get(profile);
  auto* extension_prefs = ExtensionPrefs::Get(profile);

  if (extensions_size == 1) {
    const Extension* extension =
        extension_registry->disabled_extensions().GetByID(extension_ids[0]);
    CHECK(extension);
    CHECK(extension_prefs->HasDisableReason(
        extension_ids[0],
        disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION));

    dialog_builder.SetTitle(l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_TITLE,
        base::UTF8ToUTF16(extension->name())));
  } else {
    dialog_builder.SetTitle(l10n_util::GetStringFUTF16Int(
        IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_PLURAL_TITLE,
        extensions_size));

    for (const auto& extension_id : extension_ids) {
      const Extension* extension =
          extension_registry->disabled_extensions().GetByID(extension_id);
      CHECK(extension);
      CHECK(extension_prefs->HasDisableReason(
          extension_ids[0],
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION));

      dialog_builder.AddParagraph(
          ui::DialogModelLabel(base::UTF8ToUTF16(extension->name())));
    }
  }

  ExtensionsToolbarContainer* const extensions_container =
      parent ? GetExtensionsToolbarContainer(parent) : nullptr;
  DCHECK(extensions_container);

  ShowDialog(extensions_container, extension_ids, dialog_builder.Build());
}

}  // namespace extensions
