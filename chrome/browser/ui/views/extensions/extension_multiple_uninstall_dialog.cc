// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

void ShowExtensionMultipleUninstallDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    const std::vector<ExtensionId>& extension_ids,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  auto split_cancel_callback =
      base::SplitOnceCallback(std::move(cancel_callback));
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetInternalName("ExtensionMultipleUninstallDialog")
      .SetTitle(l10n_util::GetPluralStringFUTF16(
          IDS_EXTENSION_PROMPT_MULTIPLE_UNINSTALL_TITLE, extension_ids.size()))
      .OverrideShowCloseButton(false)
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON)))
      .AddCancelButton(std::move(split_cancel_callback.first))
      .SetCloseActionCallback(std::move(split_cancel_callback.second));

  for (const auto& extension_id : extension_ids) {
    // Retrieve the extension info.
    const Extension* current_extension =
        ExtensionRegistry::Get(profile)->GetExtensionById(
            extension_id, ExtensionRegistry::EVERYTHING);
    CHECK(current_extension);
    std::string dialog_label =
        l10n_util::GetStringUTF8(
            IDS_EXTENSION_PROMPT_MULTIPLE_UNINSTALL_NAME_BULLET_POINT) +
        current_extension->name();
    dialog_builder.AddParagraph(
        ui::DialogModelLabel(base::UTF8ToUTF16(dialog_label)));
  }
  std::unique_ptr<ui::DialogModel> dialog_model = dialog_builder.Build();

  ExtensionsToolbarContainer* const container =
      parent ? GetExtensionsToolbarContainer(parent) : nullptr;
  if (container && container->GetVisible()) {
    ShowDialog(container, extension_ids, std::move(dialog_model));
  } else {
    constrained_window::ShowBrowserModal(std::move(dialog_model), parent);
  }
}

}  // namespace extensions
