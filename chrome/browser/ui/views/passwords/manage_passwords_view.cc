// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/bubble_controllers/manage_passwords_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_list_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"

using password_manager::metrics_util::PasswordManagementBubbleInteractions;

ManagePasswordsView::ManagePasswordsView(content::WebContents* web_contents,
                                         views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

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
      std::make_unique<PageSwitcherView>(std::make_unique<views::View>()));

  if (!controller_.GetCredentials().empty()) {
    // The request is cancelled when the |controller_| is destroyed.
    // |controller_| has the same lifetime as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &ManagePasswordsView::OnFaviconReady, base::Unretained(this)));
  }
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetProperty(views::kElementIdentifierKey, kTopView);
}

ManagePasswordsView::~ManagePasswordsView() = default;

void ManagePasswordsView::DisplayDetailsOfPasswordForTesting(
    password_manager::PasswordForm password_form) {
  controller_.set_details_bubble_credential(std::move(password_form));
  RecreateLayout();
}

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
  if (controller_.bubble_mode() ==
      ManagePasswordsBubbleController::BubbleMode::kSingleCredentialDetails) {
    // The user is expected to be authenticated before showing the bubble in
    // the single credential mode. Analogous to authentication expiration
    // after clicking on a credentail from the list, start the timer to close
    // the bubble.
    auth_timer_.Start(FROM_HERE,
                      password_manager::constants::kPasswordManagerAuthValidity,
                      base::BindRepeating(&ManagePasswordsView::CloseBubble,
                                          base::Unretained(this)));
  }

  RecreateLayout();
}

bool ManagePasswordsView::Accept() {
  // Accept button is only visible in the details page.
  DCHECK(password_details_view_);
  DCHECK(controller_.get_details_bubble_credential().has_value());
  password_manager::PasswordForm updated_form =
      controller_.get_details_bubble_credential().value();
  std::optional<std::u16string> updated_username =
      password_details_view_->GetUserEnteredUsernameValue();
  if (updated_username.has_value()) {
    updated_form.username_value = updated_username.value();
  }
  std::optional<std::u16string> updated_note =
      password_details_view_->GetUserEnteredPasswordNoteValue();
  if (updated_note.has_value()) {
    updated_form.SetNoteWithEmptyUniqueDisplayName(updated_note.value());
  }
  controller_.UpdateDetailsBubbleCredentialInPasswordStore(
      std::move(updated_form));
  SwitchToReadingMode();
  // Return false such that the bubble doesn't get closed upon clicking the
  // button.
  return false;
}

bool ManagePasswordsView::Cancel() {
  // Cancel button is only visible in the details page.
  DCHECK(controller_.get_details_bubble_credential().has_value());
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
          &ManagePasswordsView::AuthenticateUserAndDisplayDetailsOf,
          base::Unretained(this)),
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->controller_.OnManageClicked(
                password_manager::ManagePasswordsReferrer::
                    kManagePasswordsBubble);
            view->CloseBubble();
            // TODO(b/329572483): move this logging to the controller.
            password_manager::metrics_util::
                LogUserInteractionsInPasswordManagementBubble(
                    PasswordManagementBubbleInteractions::
                        kManagePasswordsButtonClicked);
          },
          base::Unretained(this)),
      controller_.IsOptedInForAccountStorage());
}

std::unique_ptr<ManagePasswordsDetailsView>
ManagePasswordsView::CreatePasswordDetailsView() {
  DCHECK(controller_.get_details_bubble_credential().has_value());
  return std::make_unique<ManagePasswordsDetailsView>(
      controller_.get_details_bubble_credential().value(),
      /*allow_empty_username_edit=*/controller_.bubble_mode() ==
          ManagePasswordsBubbleController::BubbleMode::kCredentialList,
      base::BindRepeating(&ManagePasswordsBubbleController::UsernameExists,
                          base::Unretained(&controller_)),
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->SetButtons(
                static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));
            view->SetButtonLabel(
                ui::mojom::DialogButton::kOk,
                l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UPDATE));
            view->GetBubbleFrameView()->SetFootnoteView(
                view->CreateFooterView());
            view->PreferredSizeChanged();
          },
          base::Unretained(this)),
      base::BindRepeating(&ManagePasswordsView::ExtendAuthValidity,
                          base::Unretained(this)),
      base::BindRepeating(
          [](ManagePasswordsView* view, bool is_invalid) {
            view->SetButtonEnabled(ui::mojom::DialogButton::kOk, !is_invalid);
          },
          base::Unretained(this)),
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->controller_.OnManagePasswordClicked(
                password_manager::ManagePasswordsReferrer::
                    kManagePasswordDetailsBubble);
            view->CloseBubble();
            // TODO(b/329572483): move this logging to the controller.
            password_manager::metrics_util::
                LogUserInteractionsInPasswordManagementBubble(
                    PasswordManagementBubbleInteractions::
                        kManagePasswordButtonClicked);
          },
          base::Unretained(this)));
}

std::unique_ptr<views::View> ManagePasswordsView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](ManagePasswordsView* dialog) {
        dialog->controller_.OnGooglePasswordManagerLinkClicked();
        // TODO(b/329572483): move this logging to the controller.
        password_manager::metrics_util::
            LogUserInteractionsInPasswordManagementBubble(
                PasswordManagementBubbleInteractions::
                    kGooglePasswordManagerLinkClicked);
      },
      base::Unretained(this));

  switch (controller_.GetPasswordSyncState()) {
    case ManagePasswordsBubbleController::SyncState::kNotActive:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SAVING_ON_DEVICE,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE,
          open_password_manager_closure, views::style::CONTEXT_BUBBLE_FOOTER);
    case ManagePasswordsBubbleController::SyncState::
        kActiveWithSyncFeatureEnabled:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          controller_.GetPrimaryAccountEmail(), open_password_manager_closure,
          views::style::CONTEXT_BUBBLE_FOOTER);
    case ManagePasswordsBubbleController::SyncState::
        kActiveWithAccountPasswords:
      // Account store users have a special footer in the management bubble
      // since they might have a mix of synced and non-synced passwords.
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_MANAGEMENT_BUBBLE_FOOTER_ACCOUNT_STORE_USERS,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          open_password_manager_closure, views::style::CONTEXT_BUBBLE_FOOTER);
  }
}

std::unique_ptr<views::View>
ManagePasswordsView::CreateMovePasswordFooterView() {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  base::RepeatingClosure move_password_closure = base::BindRepeating(
      [](ManagePasswordsView* dialog) {
        dialog->controller_.OnMovePasswordLinkClicked();
      },
      base::Unretained(this));

  auto footer = std::make_unique<views::FlexLayoutView>();

  views::ImageView* icon_view = footer->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kSaveCloudIcon, ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));
  icon_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  icon_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0, 0,
          layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left()));

  views::StyledLabel* footer_label =
      footer->AddChildView(CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_MANAGER_MANAGEMENT_BUBBLE_FOOTER_MOVE_PASSWORD,
          /*link_message_id=*/
          IDS_PASSWORD_MANAGER_MANAGEMENT_BUBBLE_LINK_TEXT_MOVE_PASSWORD,
          move_password_closure, views::style::CONTEXT_BUBBLE_FOOTER));

  const int footer_label_width =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      2 * layout_provider->GetInsetsMetric(views::INSETS_DIALOG).width();
  footer_label->SizeToFit(footer_label_width);

  return footer;
}

void ManagePasswordsView::RecreateLayout() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  CHECK(frame_view);
  frame_view->SetFootnoteView(nullptr);
  if (controller_.get_details_bubble_credential().has_value()) {
    bool has_back_button =
        controller_.bubble_mode() ==
        ManagePasswordsBubbleController::BubbleMode::kCredentialList;
    frame_view->SetTitleView(ManagePasswordsDetailsView::CreateTitleView(
        controller_.get_details_bubble_credential().value(),
        has_back_button ? std::make_optional(base::BindRepeating(
                              &ManagePasswordsView::SwitchToListView,
                              base::Unretained(this)))
                        : std::nullopt));
    std::unique_ptr<ManagePasswordsDetailsView> details_view =
        CreatePasswordDetailsView();
    password_details_view_ = details_view.get();
    page_container_->SwitchToPage(std::move(details_view));
    if (controller_.IsOptedInForAccountStorage() &&
        !controller_.get_details_bubble_credential()
             .value()
             .IsUsingAccountStore()) {
      frame_view->SetFootnoteView(CreateMovePasswordFooterView());
      frame_view->SetProperty(views::kElementIdentifierKey, kFooterId);
    }
  } else {
    password_details_view_ = nullptr;
    frame_view->SetTitleView(CreateTitleView(controller_.GetTitle()));
    page_container_->SwitchToPage(CreatePasswordListView());
    page_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_bottom(ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)));
  }
  SetTitle(controller_.GetTitle());
  PreferredSizeChanged();
}

void ManagePasswordsView::SwitchToReadingMode() {
  password_details_view_->SwitchToReadingMode();
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  RecreateLayout();
}

void ManagePasswordsView::SwitchToListView() {
  auth_timer_.Stop();
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  controller_.set_details_bubble_credential(std::nullopt);
  RecreateLayout();
}

void ManagePasswordsView::ExtendAuthValidity() {
  if (auth_timer_.IsRunning()) {
    auth_timer_.Reset();
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

void ManagePasswordsView::AuthenticateUserAndDisplayDetailsOf(
    password_manager::PasswordForm password_form) {
  // Prevent the bubble from closing for the duration of the lifetime of the
  // `pin`. This is to keep it open while the user authentication is in action.
  std::unique_ptr<CloseOnDeactivatePin> pin = PreventCloseOnDeactivate();
  // Pass `pin` to the callback to keep it alive till the completion of the
  // authentication process.
  controller_.AuthenticateUserAndDisplayDetailsOf(
      std::move(password_form),
      base::BindOnce(
          [](ManagePasswordsView* view,
             std::unique_ptr<CloseOnDeactivatePin> pin,
             bool authentication_result) {
            // If the authentication is successful, navigate to the details page
            // by recreating the layout.
            if (authentication_result) {
              view->RecreateLayout();
              view->auth_timer_.Start(
                  FROM_HERE,
                  password_manager::constants::kPasswordManagerAuthValidity,
                  base::BindRepeating(&ManagePasswordsView::SwitchToListView,
                                      base::Unretained(view)));
            }

            // This is necessary on Windows since the bubble isn't activated
            // again after the conlusion of the auth flow.
            view->GetWidget()->Activate();
            // Delay the destruction of `pin` for 1 sec to make sure the bubble
            // remains open till the OS closes the authentication dialog and
            // reactivates the bubble.
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce([](std::unique_ptr<CloseOnDeactivatePin> pin) {},
                               std::move(pin)),
                base::Seconds(1));
          },
          base::Unretained(this), std::move(pin)));
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsView, kTopView);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsView, kFooterId);

BEGIN_METADATA(ManagePasswordsView)
END_METADATA
