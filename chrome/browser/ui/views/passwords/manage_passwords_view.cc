// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

ManagePasswordsView::ManagePasswordsView(content::WebContents* web_contents,
                                         views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kRevampedPasswordManagementBubble));
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Set the right and left margins to 0 such that the `page_container_` fills
  // the whole page bubble width.
  set_margins(ChromeLayoutProvider::Get()
                  ->GetInsetsMetric(views::INSETS_DIALOG)
                  .set_left_right(0, 0));

  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(CreatePasswordListView()));

  if (!controller_.local_credentials().empty()) {
    // The request is cancelled when the |controller_| is destroyed.
    // |controller_| has the same lifetime as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &ManagePasswordsView::OnFaviconReady, base::Unretained(this)));
  }

  SetFootnoteView(CreateFooterView());
}

ManagePasswordsView::~ManagePasswordsView() = default;

PasswordBubbleControllerBase* ManagePasswordsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* ManagePasswordsView::GetController() const {
  return &controller_;
}

ui::ImageModel ManagePasswordsView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void ManagePasswordsView::AddedToWidget() {
  // Since PasswordBubbleViewBase creates the bubble using
  // BubbleDialogDelegateView::CreateBubble() *after* the construction of the
  // ManagePasswordsView, the title view cannot be set in the constructor.
  GetBubbleFrameView()->SetTitleView(CreatePasswordListTitleView());
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListTitleView()
    const {
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
  // TODO(crbug.com/1382017): refactor to use the title provided by the
  // controller instead.
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      u"Saved passwords for this site"));
  return header;
}

std::unique_ptr<views::View>
ManagePasswordsView::CreatePasswordDetailsTitleView() {
  DCHECK(currently_selected_password_.has_value());
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icons and title similar to the default behavior
  // in BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->currently_selected_password_ = absl::nullopt;
            view->RecreateLayout();
          },
          base::Unretained(this)),
      vector_icons::kArrowBackIcon);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  views::InstallCircleHighlightPathGenerator(back_button.get());
  header->AddChildView(std::move(back_button));

  header->AddChildView(
      std::make_unique<views::ImageView>(GetFaviconImageModel()));

  std::string shown_origin = password_manager::GetShownOriginAndLinkUrl(
                                 currently_selected_password_.value())
                                 .first;
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      base::UTF8ToUTF16(shown_origin)));
  return header;
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListView() {
  auto container_view = std::make_unique<views::BoxLayoutView>();
  container_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  for (const password_manager::PasswordForm& password_form :
       controller_.local_credentials()) {
    // TODO(crbug.com/1382017): Make sure the alignment works for different use
    // cases. (e.g. long username, federated credentials)
    RichHoverButton* row =
        container_view->AddChildView(std::make_unique<RichHoverButton>(
            base::BindRepeating(
                [](ManagePasswordsView* view,
                   const password_manager::PasswordForm& password_form) {
                  view->currently_selected_password_ = password_form;
                  view->RecreateLayout();
                },
                base::Unretained(this), password_form),
            /*main_image_icon=*/GetFaviconImageModel(),
            /*title_text=*/GetDisplayUsername(password_form),
            /*secondary_text=*/GetDisplayPassword(password_form),
            /*tooltip_text=*/std::u16string(),
            /*subtitle_text=*/std::u16string(),
            /*action_image_icon=*/
            ui::ImageModel::FromVectorIcon(vector_icons::kSubmenuArrowIcon,
                                           ui::kColorIcon),
            /*state_icon=*/absl::nullopt));

    views::Label* password_label = row->secondary_label();
    if (password_form.federation_origin.opaque()) {
      password_label->SetTextStyle(STYLE_SECONDARY_MONOSPACED);
      password_label->SetObscured(true);
      password_label->SetElideBehavior(gfx::TRUNCATE);
    } else {
      password_label->SetTextStyle(views::style::STYLE_SECONDARY);
      password_label->SetElideBehavior(gfx::ELIDE_HEAD);
    }
  }
  return container_view;
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordDetailsView()
    const {
  DCHECK(currently_selected_password_.has_value());
  NOTIMPLEMENTED();
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View> ManagePasswordsView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](ManagePasswordsView* dialog) {
        dialog->controller_.OnGooglePasswordManagerLinkClicked();
      },
      base::Unretained(this));

  switch (controller_.GetPasswordSyncState()) {
    case password_manager::SyncState::kNotSyncing:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SAVING_ON_DEVICE,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE,
          open_password_manager_closure);
    case password_manager::SyncState::kSyncingNormalEncryption:
    case password_manager::SyncState::kSyncingWithCustomPassphrase:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          controller_.GetPrimaryAccountEmail(), open_password_manager_closure);
    case password_manager::SyncState::kAccountPasswordsActiveNormalEncryption:
      // Account store users have a special footer in the management bubble
      // since they might have a mix of synced and non-synced passwords.
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_MANAGEMENT_BUBBLE_FOOTER_ACCOUNT_STORE_USERS,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          open_password_manager_closure);
  }
}

void ManagePasswordsView::RecreateLayout() {
  DCHECK(GetBubbleFrameView());
  if (currently_selected_password_.has_value()) {
    GetBubbleFrameView()->SetTitleView(CreatePasswordDetailsTitleView());
    page_container_->SwitchToPage(CreatePasswordDetailsView());
  } else {
    GetBubbleFrameView()->SetTitleView(CreatePasswordListTitleView());
    page_container_->SwitchToPage(CreatePasswordListView());
  }
}

void ManagePasswordsView::OnFaviconReady(const gfx::Image& favicon) {
  if (!favicon.IsEmpty()) {
    favicon_ = favicon;
    RecreateLayout();
  }
}

ui::ImageModel ManagePasswordsView::GetFaviconImageModel() const {
  // Use a globe fallback icon until the actual favicon is loaded.
  return favicon_.IsEmpty() ? ui::ImageModel::FromVectorIcon(
                                  kGlobeIcon, ui::kColorIcon, gfx::kFaviconSize)
                            : ui::ImageModel::FromImage(favicon_);
}
