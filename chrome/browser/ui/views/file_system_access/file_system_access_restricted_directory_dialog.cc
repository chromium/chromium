// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restricted_directory_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace {

using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessRestrictedDirectoryDialog(
    Browser* const browser,
    const url::Origin& origin,
    HandleType handle_type,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto accept_callback = base::BindOnce(std::move(split_callback.first),
                                        SensitiveEntryResult::kTryAgain);
  // Further split the cancel callback, which we need to pass to two different
  // builder methods.
  auto cancel_callbacks = base::SplitOnceCallback(base::BindOnce(
      std::move(split_callback.second), SensitiveEntryResult::kAbort));

  std::u16string origin_or_short_name =
      file_system_access_ui_helper::GetFormattedOriginOrAppShortName(browser,
                                                                     origin);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder
      .SetTitle(l10n_util::GetStringUTF16(
          handle_type == HandleType::kDirectory
              ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_TITLE
              : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_TITLE))
      .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
          handle_type == HandleType::kDirectory
              ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_TEXT
              : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_TEXT,
          ui::DialogModelLabel::CreateEmphasizedText(origin_or_short_name)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModelButton::Params().SetLabel(l10n_util::GetStringUTF16(
              handle_type == HandleType::kDirectory
                  ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_BUTTON
                  : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_BUTTON)))
      .AddCancelButton(std::move(cancel_callbacks.first))
      .SetCloseActionCallback(std::move(cancel_callbacks.second));
  return dialog_builder.Build();
}

}  // namespace

void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    HandleType handle_type,
    base::OnceCallback<void(SensitiveEntryResult)> callback,
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  constrained_window::ShowWebModal(
      CreateFileSystemAccessRestrictedDirectoryDialog(
          browser, origin, handle_type, std::move(callback)),
      web_contents);
}

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessRestrictedDirectoryDialogForTesting(  // IN-TEST
    const url::Origin& origin,
    HandleType handle_type,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  return CreateFileSystemAccessRestrictedDirectoryDialog(
      /*browser=*/nullptr, origin, handle_type, std::move(callback));
}
