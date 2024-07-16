// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_list_view.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_class_properties.h"

using password_manager::metrics_util::PasswordManagementBubbleInteractions;

ManagePasswordsListView::ManagePasswordsListView(
    base::span<std::unique_ptr<password_manager::PasswordForm> const>
        credentials,
    ui::ImageModel favicon,
    base::RepeatingCallback<void(password_manager::PasswordForm)>
        on_row_clicked_callback,
    base::RepeatingClosure on_navigate_to_settings_clicked_callback,
    bool is_account_storage_available) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  for (const std::unique_ptr<password_manager::PasswordForm>& password_form :
       credentials) {
    std::optional<ui::ImageModel> store_icon = std::nullopt;
    if (is_account_storage_available && !password_form->IsUsingAccountStore()) {
      store_icon = ui::ImageModel::FromVectorIcon(
          vector_icons::kNotUploadedIcon, ui::kColorIcon, gfx::kFaviconSize);
    }

    std::unique_ptr<RichHoverButton> list_item =
        std::make_unique<RichHoverButton>(
            base::BindRepeating(
                [](base::RepeatingCallback<void(password_manager::PasswordForm)>
                       on_row_clicked_callback,
                   const password_manager::PasswordForm& password_form) {
                  on_row_clicked_callback.Run(password_form);
                  PasswordManagementBubbleInteractions user_interaction =
                      password_form.GetNoteWithEmptyUniqueDisplayName().empty()
                          ? PasswordManagementBubbleInteractions::
                                kCredentialRowWithoutNoteClicked
                          : PasswordManagementBubbleInteractions::
                                kCredentialRowWithNoteClicked;
                  password_manager::metrics_util::
                      LogUserInteractionsInPasswordManagementBubble(
                          user_interaction);
                },
                on_row_clicked_callback, *password_form),
            /*main_image_icon=*/favicon,
            /*title_text=*/GetDisplayUsername(*password_form),
            /*secondary_text=*/std::u16string(),
            /*tooltip_text=*/std::u16string(),
            /*subtitle_text=*/std::u16string(),
            /*action_image_icon=*/
            ui::ImageModel::FromVectorIcon(vector_icons::kSubmenuArrowIcon,
                                           ui::kColorIcon),
            /*state_icon=*/store_icon);

    if (is_account_storage_available && !password_form->IsUsingAccountStore()) {
      list_item->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_MANAGEMENT_BUBBLE_LIST_ITEM_DEVICE_ONLY_ACCESSIBLE_TEXT,
          GetDisplayUsername(*password_form)));
    }

    // TODO(crbug.com/40245430): Add a tooltip if needed.
    AddChildView(std::move(list_item));
  }

  AddChildView(std::make_unique<views::Separator>())
      ->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_CONTENT_LIST_VERTICAL_SINGLE),
                          0)));

  auto* manage_passwords_button =
      AddChildView(std::make_unique<RichHoverButton>(
          std::move(on_navigate_to_settings_clicked_callback),
          /*main_image_icon=*/
          ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                         ui::kColorIcon),
          /*title_text=*/
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON),
          /*secondary_text=*/std::u16string(),
          /*tooltip_text=*/
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON),
          /*subtitle_text=*/std::u16string(),
          /*action_image_icon=*/
          ui::ImageModel::FromVectorIcon(
              vector_icons::kLaunchIcon, ui::kColorIconSecondary,
              GetLayoutConstant(PAGE_INFO_ICON_SIZE)),
          /*state_icon=*/std::nullopt));
  manage_passwords_button->SetID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kManagePasswordsButton));

  SetProperty(views::kElementIdentifierKey, kTopView);
}

ManagePasswordsListView::~ManagePasswordsListView() = default;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsListView, kTopView);

BEGIN_METADATA(ManagePasswordsListView)
END_METADATA
