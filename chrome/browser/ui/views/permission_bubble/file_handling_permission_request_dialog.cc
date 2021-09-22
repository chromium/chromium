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
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
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

  const bool plural = file_paths.size() > 1;
  SetTitle(l10n_util::GetStringUTF16(
      plural ? IDS_WEB_APP_FILE_HANDLING_PERMISSION_TITLE_MULTIPLE
             : IDS_WEB_APP_FILE_HANDLING_PERMISSION_TITLE));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      layout_provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // Description of permission.
  const GURL app_url =
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
  auto* description_label =
      AddChildView(std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
          plural ? IDS_WEB_APP_FILE_HANDLING_PERMISSION_DESCRIPTION_MULTIPLE
                 : IDS_WEB_APP_FILE_HANDLING_PERMISSION_DESCRIPTION,
          url_formatter::FormatUrlForSecurityDisplay(
              app_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC))));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // File icon and name list.
  auto* files_view = AddChildView(std::make_unique<views::View>());
  files_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // TODO(tluk): We should be sourcing the size of the file icon from the layout
  // provider rather than relying on hardcoded constants.
  constexpr int kIconSize = 16;
  auto* icon = files_view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kDescriptionIcon, ui::kColorIcon, kIconSize)));
  const int icon_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  icon->SetProperty(views::kMarginsKey, gfx::Insets(0, 0, 0, icon_margin));

  // File name list.
  std::vector<std::u16string> file_names;
  // Display at most a dozen files. After that, elide.
  size_t displayed_file_name_count =
      std::min(file_paths.size(), static_cast<size_t>(12));
  file_names.reserve(displayed_file_name_count + 1);

  // Additionally, elide very long file names (the max width is the width
  // available for the label).
  const int available_width = fixed_width() -
                              box_layout->inside_border_insets().width() -
                              icon->GetPreferredSize().width() - icon_margin;
  std::transform(file_paths.begin(),
                 file_paths.begin() + displayed_file_name_count,
                 std::back_inserter(file_names),
                 [available_width](const base::FilePath& file_path) {
                   // Use slightly less than the available width since some
                   // space is needed for the separator.
                   return gfx::ElideFilename(file_path.BaseName(),
                                             views::Label::GetDefaultFontList(),
                                             0.95 * available_width);
                 });
  if (file_paths.size() > displayed_file_name_count)
    file_names.emplace_back(std::u16string(gfx::kEllipsisUTF16));

  auto* files_label =
      files_view->AddChildView(std::make_unique<views::Label>(base::JoinString(
          file_names, l10n_util::GetStringUTF16(
                          IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR))));
  files_label->SetMultiLine(true);
  files_label->SetMaximumWidth(available_width);
  files_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Permanency checkbox.
  auto checkbox = std::make_unique<views::Checkbox>();
  bool multiple_associations = false;
  std::u16string associations =
      web_app::GetFileTypeAssociationsHandledByWebAppsForDisplay(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          app_url, &multiple_associations);
  checkbox->SetText(l10n_util::GetStringFUTF16(
      multiple_associations
          ? IDS_WEB_APP_FILE_HANDLING_PERMISSION_STICKY_CHOICE_MULTIPLE
          : IDS_WEB_APP_FILE_HANDLING_PERMISSION_STICKY_CHOICE,
      associations));
  checkbox->SetMultiLine(true);
  checkbox_ = AddChildView(std::move(checkbox));

  // Dialog buttons.
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_WEB_APP_PERMISSION_DONT_ALLOW));
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  SetAcceptCallback(
      base::BindOnce(&FileHandlingPermissionRequestDialog::OnDialogAccepted,
                     base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&FileHandlingPermissionRequestDialog::OnDialogCanceled,
                     base::Unretained(this)));

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
