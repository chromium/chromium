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
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"

using password_manager::metrics_util::PasswordManagementBubbleInteractions;

// static
std::unique_ptr<views::View> ManagePasswordsListView::CreateTitleView(
    const std::u16string& title) {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icon and title similar to the default behavior in
  // BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());
  header->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(title));
  return header;
}

ManagePasswordsListView::ManagePasswordsListView(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        credentials,
    ui::ImageModel favicon,
    base::RepeatingCallback<void(password_manager::PasswordForm)>
        on_row_clicked_callback,
    base::RepeatingClosure on_navigate_to_settings_clicked_callback) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  for (const std::unique_ptr<password_manager::PasswordForm>& password_form :
       credentials) {
    absl::optional<ui::ImageModel> store_icon = absl::nullopt;
    if (password_form->IsUsingAccountStore()) {
      store_icon = ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          vector_icons::kGoogleGLogoIcon,
#else
          vector_icons::kSyncIcon,
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
          gfx::kPlaceholderColor, gfx::kFaviconSize);
    }
    // TODO(crbug.com/1382017): Add a tooltip if needed.
    AddChildView(std::make_unique<RichHoverButton>(
        base::BindRepeating(
            [](base::RepeatingCallback<void(password_manager::PasswordForm)>
                   on_row_clicked_callback,
               const password_manager::PasswordForm& password_form) {
              on_row_clicked_callback.Run(password_form);
              password_manager::metrics_util::
                  LogUserInteractionsInPasswordManagementBubble(
                      PasswordManagementBubbleInteractions::
                          kCredentialRowClicked);
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
        /*state_icon=*/store_icon));
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
          /*state_icon=*/absl::nullopt));
  manage_passwords_button->SetID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kManagePasswordsButton));
}

ManagePasswordsListView::~ManagePasswordsListView() = default;
