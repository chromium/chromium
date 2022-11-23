// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_permission_dialog.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/permission_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);

namespace {

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;

int GetMessageText(const RequestData& request) {
  switch (request.access) {
    case AccessType::kRead:
      if (base::FeatureList::IsEnabled(
              features::kFileSystemAccessPersistentPermissions)) {
        return request.handle_type == HandleType::kDirectory
                   ? IDS_FILE_SYSTEM_ACCESS_READ_PERMISSION_DIRECTORY_TEXT
                   : IDS_FILE_SYSTEM_ACCESS_READ_PERMISSION_FILE_TEXT;
      } else {
        return request.handle_type == HandleType::kDirectory
                   ? IDS_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_READ_PERMISSION_DIRECTORY_TEXT
                   : IDS_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_READ_PERMISSION_FILE_TEXT;
      }
    case AccessType::kWrite:
    case AccessType::kReadWrite:
      // Only difference between write and read-write access dialog is in button
      // label and dialog title.
      if (base::FeatureList::IsEnabled(
              features::kFileSystemAccessPersistentPermissions)) {
        return request.handle_type == HandleType::kDirectory
                   ? IDS_FILE_SYSTEM_ACCESS_WRITE_PERMISSION_DIRECTORY_TEXT
                   : IDS_FILE_SYSTEM_ACCESS_WRITE_PERMISSION_FILE_TEXT;
      } else {
        return request.handle_type == HandleType::kDirectory
                   ? IDS_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_WRITE_PERMISSION_DIRECTORY_TEXT
                   : IDS_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_WRITE_PERMISSION_FILE_TEXT;
      }
  }
  NOTREACHED();
}

int GetButtonLabel(const RequestData& request) {
  switch (request.access) {
    case AccessType::kRead:
      return request.handle_type == HandleType::kDirectory
                 ? IDS_FILE_SYSTEM_ACCESS_VIEW_DIRECTORY_PERMISSION_ALLOW_TEXT
                 : IDS_FILE_SYSTEM_ACCESS_VIEW_FILE_PERMISSION_ALLOW_TEXT;
    case AccessType::kWrite:
      return IDS_FILE_SYSTEM_ACCESS_WRITE_PERMISSION_ALLOW_TEXT;
    case AccessType::kReadWrite:
      return request.handle_type == HandleType::kDirectory
                 ? IDS_FILE_SYSTEM_ACCESS_EDIT_DIRECTORY_PERMISSION_ALLOW_TEXT
                 : IDS_FILE_SYSTEM_ACCESS_EDIT_FILE_PERMISSION_ALLOW_TEXT;
  }
  NOTREACHED();
}

std::u16string GetWindowTitle(const RequestData& request) {
  switch (request.access) {
    case AccessType::kRead:
      if (request.handle_type == HandleType::kDirectory) {
        return l10n_util::GetStringUTF16(
            IDS_FILE_SYSTEM_ACCESS_READ_DIRECTORY_PERMISSION_TITLE);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_FILE_SYSTEM_ACCESS_READ_FILE_PERMISSION_TITLE,
            file_system_access_ui_helper::GetPathForDisplay(request.path));
      }
    case AccessType::kWrite:
      return l10n_util::GetStringFUTF16(
          IDS_FILE_SYSTEM_ACCESS_WRITE_PERMISSION_TITLE,
          file_system_access_ui_helper::GetPathForDisplay(request.path));
    case AccessType::kReadWrite:
      if (request.handle_type == HandleType::kDirectory) {
        return l10n_util::GetStringUTF16(
            IDS_FILE_SYSTEM_ACCESS_EDIT_DIRECTORY_PERMISSION_TITLE);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_FILE_SYSTEM_ACCESS_EDIT_FILE_PERMISSION_TITLE,
            file_system_access_ui_helper::GetPathForDisplay(request.path));
      }
  }
  NOTREACHED();
}

std::unique_ptr<ui::DialogModel> CreateFileSystemAccessPermissionDialog(
    Browser* const browser,
    const RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto accept_callback = base::BindOnce(std::move(split_callback.first),
                                        permissions::PermissionAction::GRANTED);
  // Further split the cancel callback, which we need to pass to two different
  // builder methods.
  auto cancel_callbacks = base::SplitOnceCallback(
      base::BindOnce(std::move(split_callback.second),
                     permissions::PermissionAction::DISMISSED));

  auto origin_or_short_name =
      file_system_access_ui_helper::GetFormattedOriginOrAppShortName(
          browser, request.origin);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(GetWindowTitle(request))
      .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
          GetMessageText(request),
          {ui::DialogModelLabel::CreateEmphasizedText(origin_or_short_name),
           ui::DialogModelLabel::CreateEmphasizedText(
               file_system_access_ui_helper::GetPathForDisplay(request.path))}))
      .AddOkButton(std::move(accept_callback),
                   ui::DialogModelButton::Params().SetLabel(
                       l10n_util::GetStringUTF16(GetButtonLabel(request))))
      .AddCancelButton(std::move(cancel_callbacks.first),
                       ui::DialogModelButton::Params().SetId(kCancelButtonId))
      .SetCloseActionCallback(std::move(cancel_callbacks.second))
      .SetInitiallyFocusedField(kCancelButtonId);
  return dialog_builder.Build();
}

}  // namespace

void ShowFileSystemAccessPermissionDialog(
    const RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  constrained_window::ShowWebModal(CreateFileSystemAccessPermissionDialog(
                                       browser, request, std::move(callback)),
                                   web_contents);
}

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessPermissionDialogForTesting(  // IN-TEST
    const RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback) {
  return CreateFileSystemAccessPermissionDialog(/*browser=*/nullptr, request,
                                                std::move(callback));
}
