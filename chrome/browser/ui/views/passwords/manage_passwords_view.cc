// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_list_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_class_properties.h"

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

  // Title insets assume there is content (and thus have no bottom padding). Use
  // dialog insets to get the bottom margin back.
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  // Set the right and left margins to 0 such that the `page_container_` fills
  // the whole page bubble width. Top margin is handled by the title above, and
  // remove bottom margin such that `page_container_` can assign it if needed.
  set_margins(gfx::Insets());

  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(CreatePasswordListView()));

  if (!controller_.GetCredentials().empty()) {
    // The request is cancelled when the |controller_| is destroyed.
    // |controller_| has the same lifetime as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &ManagePasswordsView::OnFaviconReady, base::Unretained(this)));
  }
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
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
  GetBubbleFrameView()->SetTitleView(
      ManagePasswordsListView::CreateTitleView());
}

bool ManagePasswordsView::Accept() {
  // Accept button is only visible in the details page where a password is
  // selected.
  DCHECK(password_details_view_);
  DCHECK(currently_selected_password_.has_value());
  password_manager::PasswordForm updated_form =
      currently_selected_password_.value();
  absl::optional<std::u16string> updated_username =
      password_details_view_->GetUserEnteredUsernameValue();
  if (updated_username.has_value()) {
    updated_form.username_value = updated_username.value();
  }
  absl::optional<std::u16string> updated_note =
      password_details_view_->GetUserEnteredPasswordNoteValue();
  if (updated_note.has_value()) {
    updated_form.SetNoteWithEmptyUniqueDisplayName(updated_note.value());
  }
  controller_.UpdateStoredCredential(currently_selected_password_.value(),
                                     updated_form);
  currently_selected_password_ = std::move(updated_form);
  SwitchToReadingMode();
  // Return false such that the bubble doesn't get closed upon clicking the
  // button.
  return false;
}

bool ManagePasswordsView::Cancel() {
  // Cancel button is only visible in the details page where a password is
  // selected.
  DCHECK(currently_selected_password_.has_value());
  SwitchToReadingMode();
  // Return false such that the bubble doesn't get closed upon clicking the
  // button.
  return false;
}

std::unique_ptr<ManagePasswordsListView>
ManagePasswordsView::CreatePasswordListView() {
  return std::make_unique<ManagePasswordsListView>(
      controller_.GetCredentials(), GetFaviconImageModel(),
      base::BindRepeating(
          [](ManagePasswordsView* view,
             password_manager::PasswordForm password_form) {
            view->currently_selected_password_ = password_form;
            view->RecreateLayout();
          },
          base::Unretained(this)),
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->controller_.OnManageClicked(
                password_manager::ManagePasswordsReferrer::
                    kManagePasswordsBubble);
            view->CloseBubble();
          },
          base::Unretained(this)));
}

std::unique_ptr<ManagePasswordsDetailsView>
ManagePasswordsView::CreatePasswordDetailsView() {
  DCHECK(currently_selected_password_.has_value());
  return std::make_unique<ManagePasswordsDetailsView>(
      currently_selected_password_.value(),
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
            // TODO(crbug.com/1408790): use internationalized string.
            view->SetButtonLabel(ui::DIALOG_BUTTON_OK, u"Update");
            view->PreferredSizeChanged();
            view->SizeToContents();
          },
          base::Unretained(this)));
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
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  DCHECK(frame_view);

  if (currently_selected_password_.has_value()) {
    // TODO(crbug.com/1382017): implement authentication before navigating to
    // the details page.
    frame_view->SetTitleView(ManagePasswordsDetailsView::CreateTitleView(
        currently_selected_password_.value(),
        base::BindRepeating(
            [](ManagePasswordsView* view) {
              view->SetButtons(ui::DIALOG_BUTTON_NONE);
              view->currently_selected_password_ = absl::nullopt;
              view->RecreateLayout();
            },
            base::Unretained(this))));
    frame_view->SetFootnoteView(nullptr);
    std::unique_ptr<ManagePasswordsDetailsView> details_view =
        CreatePasswordDetailsView();
    password_details_view_ = details_view.get();
    page_container_->SwitchToPage(std::move(details_view));
    page_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_bottom(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .bottom()));
  } else {
    frame_view->SetTitleView(ManagePasswordsListView::CreateTitleView());
    frame_view->SetFootnoteView(CreateFooterView());
    page_container_->SwitchToPage(CreatePasswordListView());
    page_container_->SetProperty(views::kMarginsKey, gfx::Insets());
    password_details_view_ = nullptr;
  }
  PreferredSizeChanged();
  SizeToContents();
}

void ManagePasswordsView::SwitchToReadingMode() {
  password_details_view_->SwitchToReadingMode();
  SetButtons(ui::DIALOG_BUTTON_NONE);
  RecreateLayout();
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
