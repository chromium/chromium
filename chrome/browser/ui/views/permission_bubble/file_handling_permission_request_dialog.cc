// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/file_handling_permission_request_dialog.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "components/permissions/permission_uma_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "url/gurl.h"

FileHandlingPermissionRequestDialog* g_instance_for_testing = nullptr;

FileHandlingPermissionRequestDialog::FileHandlingPermissionRequestDialog(
    content::WebContents* web_contents,
    const std::vector<base::FilePath> file_paths,
    base::OnceCallback<void(bool, bool)> result_callback)
    : result_callback_(std::move(result_callback)) {
  g_instance_for_testing = this;
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // TODO(estade): this UI is preliminary. Update when mocks are finalized.
  // TODO(estade): localize strings.
  // TODO(estade): use GetLastCommittedOrigin(). See crbug.com/698985
  const GURL app_url = web_contents->GetMainFrame()->GetLastCommittedURL();
  SetTitle(url_formatter::FormatUrlForSecurityDisplay(
               app_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC) +
           u" wants to");
  SetButtonLabel(ui::DIALOG_BUTTON_OK, u"Allow");
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, u"Don't allow");
  SetAcceptCallback(
      base::BindOnce(&FileHandlingPermissionRequestDialog::OnDialogAccepted,
                     base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&FileHandlingPermissionRequestDialog::OnDialogCanceled,
                     base::Unretained(this)));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      layout_provider->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  std::vector<std::u16string> file_names;
  file_names.reserve(file_paths.size());
  std::transform(file_paths.begin(), file_paths.end(),
                 std::back_inserter(file_names),
                 [](const base::FilePath& file_path) {
                   return file_path.LossyDisplayName();
                 });
  AddChildView(std::make_unique<views::Label>(
                   u"<image> Open " + base::JoinString(file_names, u", ")))
      ->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  std::u16string checkbox_label =
      u"Don't ask again when opening these file formats in this app: " +
      web_app::GetFileTypeAssociationsHandledByWebAppsForDisplay(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          app_url);

  checkbox_ = AddChildView(std::make_unique<views::Checkbox>(checkbox_label));
  checkbox_->SetChecked(true);
  checkbox_->SetMultiLine(true);

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::FILE_HANDLING_PERMISSION_REQUEST);
}

FileHandlingPermissionRequestDialog::~FileHandlingPermissionRequestDialog() {
  if (result_callback_)
    OnDialogCanceled();
  if (g_instance_for_testing == this)
    g_instance_for_testing = nullptr;
}

void FileHandlingPermissionRequestDialog::UpdateAnchor() {}

FileHandlingPermissionRequestDialog::TabSwitchingBehavior
FileHandlingPermissionRequestDialog::GetTabSwitchingBehavior() {
  return kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
FileHandlingPermissionRequestDialog::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::MODAL_DIALOG;
}

// static
FileHandlingPermissionRequestDialog*
FileHandlingPermissionRequestDialog::GetInstanceForTesting() {
  return g_instance_for_testing;
}

void FileHandlingPermissionRequestDialog::OnDialogAccepted() {
  std::move(result_callback_)
      .Run(/*allow=*/true, /*permanently=*/checkbox_->GetChecked());
}

void FileHandlingPermissionRequestDialog::OnDialogCanceled() {
  std::move(result_callback_)
      .Run(/*allow=*/false, /*permanently=*/checkbox_->GetChecked());
}

BEGIN_METADATA(FileHandlingPermissionRequestDialog, views::DialogDelegateView)
END_METADATA
