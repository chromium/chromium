// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace extensions {

void ShowMv2DeprecationReEnableDialog(
    gfx::NativeWindow parent,
    const ExtensionId& extension_id,
    const std::string& extension_name,
    base::OnceCallback<void(bool)> done_callback) {
  // Split the callback for the accept and cancel actions.
  auto split_callback = base::SplitOnceCallback(std::move(done_callback));
  auto accept_callback =
      base::BindOnce(std::move(split_callback.first), /*did_accept=*/true);
  auto cancel_callback =
      base::BindOnce(std::move(split_callback.second), /*did_accept=*/false);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringFUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_REENABLE_DIALOG_TITLE,
              base::UTF8ToUTF16(extension_name)))
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_KEEP_DIALOG_DESCRIPTION)))
          .AddOkButton(
              std::move(accept_callback),
              ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_REENABLE_DIALOG_OK_BUTTON)))
          .AddCancelButton(std::move(cancel_callback))
          .Build();

  ShowDialog(parent, extension_id, std::move(dialog_model));
}

}  // namespace extensions
