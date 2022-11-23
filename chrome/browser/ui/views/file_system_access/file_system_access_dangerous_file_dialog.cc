// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_dangerous_file_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace {

using DangerousFileResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

std::unique_ptr<ui::DialogModel> CreateFileSystemAccessDangerousFileDialog(
    Browser* const browser,
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto accept_callback = base::BindOnce(std::move(split_callback.first),
                                        DangerousFileResult::kAllowed);
  // Further split the cancel callback, which we need to pass to two different
  // builder methods.
  auto cancel_callbacks = base::SplitOnceCallback(base::BindOnce(
      std::move(split_callback.second), DangerousFileResult::kAbort));

  std::u16string origin_or_short_name =
      file_system_access_ui_helper::GetFormattedOriginOrAppShortName(browser,
                                                                     origin);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder
      .SetTitle(l10n_util::GetStringFUTF16(
          IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_TITLE,
          file_system_access_ui_helper::GetPathForDisplay(path)))
      .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
          IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_TEXT,
          ui::DialogModelLabel::CreateEmphasizedText(origin_or_short_name)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModelButton::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_SAVE)))
      .AddCancelButton(
          std::move(cancel_callbacks.first),
          ui::DialogModelButton::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DONT_SAVE)))
      .SetCloseActionCallback(std::move(cancel_callbacks.second))
      .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_CANCEL);
  return dialog_builder.Build();
}

}  // namespace

void ShowFileSystemAccessDangerousFileDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  constrained_window::ShowWebModal(
      CreateFileSystemAccessDangerousFileDialog(browser, origin, path,
                                                std::move(callback)),
      web_contents);
}

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessDangerousFileDialogForTesting(  // IN-TEST
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback) {
  return CreateFileSystemAccessDangerousFileDialog(/*browser=*/nullptr, origin,
                                                   path, std::move(callback));
}
