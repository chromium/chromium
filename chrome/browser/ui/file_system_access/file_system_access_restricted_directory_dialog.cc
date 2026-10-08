// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_restricted_directory_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

using AccessTrigger = content::FileSystemAccessPermissionContext::AccessTrigger;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessRestrictedDirectoryDialog(
    content::WebContents* web_contents,
    const url::Origin& origin,
    HandleType handle_type,
    AccessTrigger access_trigger,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  // Drag-and-drop has no file picker to return to, so only offer dismissal.
  const bool is_drag_and_drop = access_trigger == AccessTrigger::kDragAndDrop;
  auto accept_callback =
      base::BindOnce(std::move(split_callback.first),
                     is_drag_and_drop ? SensitiveEntryResult::kAbort
                                      : SensitiveEntryResult::kTryAgain);
  auto cancel_callback = base::BindOnce(std::move(split_callback.second),
                                        SensitiveEntryResult::kAbort);

  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  std::u16string origin_identity_name =
      file_system_access_ui_helper::GetUrlIdentityName(profile,
                                                       origin.GetURL());

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
          ui::DialogModelLabel::CreateEmphasizedText(origin_identity_name)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              is_drag_and_drop
                  ? IDS_APP_OK
                  : (handle_type == HandleType::kDirectory
                         ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_BUTTON
                         : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_BUTTON))));
  if (!is_drag_and_drop) {
    auto cancel_callbacks = base::SplitOnceCallback(std::move(cancel_callback));
    dialog_builder.AddCancelButton(std::move(cancel_callbacks.first));
    cancel_callback = std::move(cancel_callbacks.second);
  }
  dialog_builder.SetCloseActionCallback(std::move(cancel_callback));
  return dialog_builder.Build();
}

}  // namespace

void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    HandleType handle_type,
    AccessTrigger access_trigger,
    base::OnceCallback<void(SensitiveEntryResult)> callback,
    content::WebContents* web_contents) {
  chrome::ShowTabModal(CreateFileSystemAccessRestrictedDirectoryDialog(
                           web_contents, origin, handle_type, access_trigger,
                           std::move(callback)),
                       web_contents);
}

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessRestrictedDirectoryDialogForTesting(  // IN-TEST
    const url::Origin& origin,
    HandleType handle_type,
    AccessTrigger access_trigger,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  return CreateFileSystemAccessRestrictedDirectoryDialog(
      /*web_contents=*/nullptr, origin, handle_type, access_trigger,
      std::move(callback));
}
