// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cstdlib>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

namespace extensions {

void ShowRequestFileSystemDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::mojom::DialogButton)> callback) {
  DCHECK(callback);

  // Split the callback for the accept and cancel actions (the later been
  // further split for cancel and close actions).
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto accept_callback = base::BindOnce(std::move(split_callback.first),
                                        ui::mojom::DialogButton::kOk);
  auto cancel_callbacks = base::SplitOnceCallback(base::BindOnce(
      std::move(split_callback.second), ui::mojom::DialogButton::kCancel));

  const std::u16string app_name = base::UTF8ToUTF16(extension_name);
  // TODO(mtomasz): Improve the dialog contents, so it's easier for the user
  // to understand what device is being requested.
  const std::u16string volume_name = base::UTF8ToUTF16(volume_label);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetAccessibleTitle(l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_TITLE))
          .AddOkButton(
              std::move(accept_callback),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_ALLOW_BUTTON)))
          .AddCancelButton(
              std::move(cancel_callbacks.first),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_DENY_BUTTON)))
          .SetCloseActionCallback(std::move(cancel_callbacks.second))
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
              writable
                  ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
                  : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
              {ui::DialogModelLabel::CreateEmphasizedText(app_name),
               ui::DialogModelLabel::CreateEmphasizedText(volume_name)}))
          .Build();

  chrome::ShowTabModal(std::move(dialog_model), web_contents);
}

}  // namespace extensions
