// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace extensions {

void ShowExtensionInstallBlockedDialog(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    content::WebContents* web_contents,
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder
      .SetTitle(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_BLOCKED_BY_POLICY_PROMPT_TITLE,
          base::UTF8ToUTF16(extension_name), base::UTF8ToUTF16(extension_id)))
      .SetIcon(ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateResizedImage(
              icon, skia::ImageOperations::ResizeMethod::RESIZE_BEST,
              gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                        extension_misc::EXTENSION_ICON_SMALL))))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_CLOSE)))
      .SetDialogDestroyingCallback(std::move(done_callback));

  if (!custom_error_message.empty()) {
    dialog_builder.AddParagraph(ui::DialogModelLabel(custom_error_message));
  }

  chrome::ShowTabModal(dialog_builder.Build(), web_contents);
}

}  // namespace extensions
