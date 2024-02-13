// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

std::u16string GetExtensionType(const extensions::Extension* extension) {
  return l10n_util::GetStringUTF16(
      extension->is_app()
          ? IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_APP
          : IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_EXTENSION);
}

std::u16string GetTitle(
    extensions::ExtensionInstalledBlockedByParentDialogAction action,
    std::u16string extension_type) {
  int title_id =
      action == extensions::ExtensionInstalledBlockedByParentDialogAction::kAdd
          ? IDS_EXTENSION_INSTALL_BLOCKED_BY_PARENT_PROMPT_TITLE
          : IDS_EXTENSION_ENABLE_BLOCKED_BY_PARENT_PROMPT_TITLE;
  return l10n_util::GetStringFUTF16(title_id, extension_type);
}
}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kParentBlockedDialogMessage);

void ShowExtensionInstallBlockedByParentDialog(
    ExtensionInstalledBlockedByParentDialogAction action,
    const extensions::Extension* extension,
    content::WebContents* web_contents,
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::u16string extension_type = GetExtensionType(extension);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(GetTitle(action, extension_type))
          .SetIcon(ui::ImageModel::FromVectorIcon(
              chromeos::kNotificationSupervisedUserIcon, ui::kColorIcon))
          .AddParagraph(
              ui::DialogModelLabel(l10n_util::GetStringUTF16(
                  IDS_EXTENSION_PERMISSIONS_BLOCKED_BY_PARENT_PROMPT_MESSAGE)),
              std::u16string(), kParentBlockedDialogMessage)
          .AddOkButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_OK)))
          .SetDialogDestroyingCallback(std::move(done_callback))
          .Build();
  gfx::NativeWindow parent_window =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;
  constrained_window::ShowBrowserModal(std::move(dialog_model), parent_window);
}

}  // namespace extensions
