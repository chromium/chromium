// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

void ShowExtensionInstallAskParentDialog(content::WebContents* web_contents,
                                         base::OnceClosure cancel_callback,
                                         base::OnceClosure approve_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Tests can auto confirm the dialog.
  switch (ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case ScopedTestDialogAutoConfirm::NONE:
      break;
    case ScopedTestDialogAutoConfirm::ACCEPT:
    case ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(approve_callback));
      return;
    case ScopedTestDialogAutoConfirm::CANCEL:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(cancel_callback));
      return;
  }

  auto split_cancel_callback =
      base::SplitOnceCallback(std::move(cancel_callback));

  ui::DialogModel::Builder dialog_builder;
  dialog_builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_EXTENSION_ASK_PARENT_PROMPT_TITLE))
      .AddParagraph(ui::DialogModelLabel(
          l10n_util::GetStringUTF16(IDS_EXTENSION_ASK_PARENT_PROMPT_BODY)))
      .AddCancelButton(std::move(split_cancel_callback.first),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CANCEL)))
      .AddOkButton(
          std::move(approve_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSION_ASK_PARENT_PROMPT_ASK_IN_PERSON_BUTTON)))
      .SetCloseActionCallback(std::move(split_cancel_callback.second));

  chrome::ShowTabModal(dialog_builder.Build(), web_contents);
}

}  // namespace extensions

#endif  // BUILDFLAG(IS_ANDROID)
