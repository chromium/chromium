// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

void ShowMv2DeprecationKeepDialog(Browser* browser,
                                  const Extension& extension,
                                  base::OnceClosure accept_callback,
                                  base::OnceClosure cancel_callback) {
  CHECK(ManifestV2ExperimentManager::Get(browser->profile())
            ->IsExtensionAffected(extension));

  auto split_cancel_callback =
      base::SplitOnceCallback(std::move(cancel_callback));
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .SetInternalName("Mv2DeprecationKeepDialog")
          .SetTitle(l10n_util::GetStringFUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_KEEP_DIALOG_TITLE,
              base::UTF8ToUTF16(extension.name())))
          .OverrideShowCloseButton(false)
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_KEEP_DIALOG_DESCRIPTION)))
          .AddOkButton(
              std::move(accept_callback),
              ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_KEEP_DIALOG_OK_BUTTON)))
          .AddCancelButton(std::move(split_cancel_callback.first))
          .SetCloseActionCallback(std::move(split_cancel_callback.second))
          .Build();

  ShowDialog(browser, std::move(dialog_model));
}

}  // namespace extensions
