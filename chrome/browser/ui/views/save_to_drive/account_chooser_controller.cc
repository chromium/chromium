// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"

#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_util.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/display/screen.h"

namespace save_to_drive {
namespace {
// Computes the bounds of the popup window. The popup window is centered in the
// source window, if the source window is large enough to contain the popup
// window. Otherwise, the popup window is centered in the screen.
gfx::Rect ComputePopupWindowBounds(content::WebContents* source_window) {
  gfx::Rect source_window_bounds = source_window->GetContainerBounds();
  const int kPopupWindowWidth = 400;
  const int kPopupWindowHeight = 484;
  int x_coordinate;
  int y_coordinate;

  if (source_window_bounds.width() >= kPopupWindowWidth &&
      source_window_bounds.height() >= kPopupWindowHeight) {
    x_coordinate = source_window_bounds.x() +
                   ((source_window_bounds.width() - kPopupWindowWidth) / 2);
    y_coordinate = source_window_bounds.y() +
                   ((source_window_bounds.height() - kPopupWindowHeight) / 2);
  } else {
    display::Screen* screen = display::Screen::Get();
    gfx::Rect source_display_bounds =
        screen->GetDisplayNearestView(source_window->GetNativeView())
            .work_area();
    x_coordinate = (source_display_bounds.width() - kPopupWindowWidth) / 2;
    y_coordinate = (source_display_bounds.height() - kPopupWindowHeight) / 2;
  }
  return gfx::Rect(x_coordinate, y_coordinate, kPopupWindowWidth,
                   kPopupWindowHeight);
}
}  // namespace

/////////////////
// ProfileInfo //
/////////////////

AccountChooserController::ProfileInfo::ProfileInfo() = default;
AccountChooserController::ProfileInfo::ProfileInfo(const ProfileInfo&) =
    default;
AccountChooserController::ProfileInfo&
AccountChooserController::ProfileInfo::operator=(const ProfileInfo&) = default;
AccountChooserController::ProfileInfo::~ProfileInfo() = default;

/////////////////////////////
// AddAccountPopupObserver //
/////////////////////////////

class AccountChooserController::AddAccountPopupObserver
    : public content::WebContentsObserver {
 public:
  explicit AddAccountPopupObserver(AccountChooserController* parent_controller)
      : parent_controller_(parent_controller) {}
  ~AddAccountPopupObserver() override = default;

  void ObservePopup(content::WebContents* popup) { Observe(popup); }
  void WebContentsDestroyed() override {
    parent_controller_->OnAddAccountPopupDestroyed();
  }

 private:
  raw_ptr<AccountChooserController> parent_controller_ = nullptr;
};

//////////////////////////////
// AccountChooserController //
//////////////////////////////

WEB_CONTENTS_USER_DATA_KEY_IMPL(AccountChooserController);

AccountChooserController::AccountChooserController(
    content::WebContents* web_contents,
    signin::IdentityManager* identity_manager)
    : WebContentsUserData<AccountChooserController>(*web_contents),
      tab_(tabs::TabInterface::MaybeGetFromContents(web_contents)),
      identity_manager_(identity_manager),
      add_account_popup_observer_(
          std::make_unique<AddAccountPopupObserver>(this)) {}

AccountChooserController::~AccountChooserController() {
  OnFlowCancelled();
}

void AccountChooserController::GetAccount(
    AccountChosenCallback on_account_chosen_callback) {
  CHECK(on_account_chosen_callback);
  scoped_identity_manager_observation_.Observe(identity_manager_);
  on_account_chosen_callback_ = std::move(on_account_chosen_callback);
  Show();
}

void AccountChooserController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  // If the account is not fully populated, then we should not show the account
  // chooser dialog.
  if (info.full_name.empty() || info.email.empty() ||
      info.account_image.IsEmpty()) {
    return;
  }
  Show();
}

void AccountChooserController::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  Show();
}

void AccountChooserController::OnAddAccountPopupDestroyed() {
  // The popup window is going away, make sure we don't keep a dangling pointer.
  // This should happen before notifying the observer, where `this` will be
  // destroyed.
  add_account_popup_ = nullptr;
  if (!add_account_popup_programatically_closed_ && !account_chooser_widget_) {
    // This means the user closed the popup window and the flow should be
    // canceled.
    OnFlowCancelled();
  }
}

void AccountChooserController::Show() {
  ProfileInfo profile_info = GetProfileInfo();
  if (profile_info.accounts.empty()) {
    CloseDialogs();
    ShowAddAccountDialog();
  } else {
    CloseAddAccountPopup();
    ShowAccountChooserDialog(std::move(profile_info));
  }
}

void AccountChooserController::ShowAccountChooserDialog(
    ProfileInfo profile_info) {
  if (account_chooser_view_) {
    account_chooser_view_->UpdateView(profile_info.accounts,
                                      profile_info.primary_account_id);
    tab_->GetTabFeatures()->tab_dialog_manager()->UpdateModalDialogBounds();
    return;
  }
  std::unique_ptr<AccountChooserView> account_chooser_view =
      std::make_unique<AccountChooserView>(this, profile_info.accounts,
                                           profile_info.primary_account_id);
  account_chooser_view_ = account_chooser_view.get();

  account_chooser_dialog_delegate_ =
      CreateDialogDelegate(std::move(account_chooser_view));

  account_chooser_widget_ =
      tab_->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
          account_chooser_dialog_delegate_.get(),
          std::make_unique<tabs::TabDialogManager::Params>());
}

void AccountChooserController::ShowAddAccountDialog() {
  if (add_account_popup_) {
    ResizeAndFocusAddAccountPopup();
    return;
  }
  content::WebContents* source_window = tab_->GetContents();
  content::OpenURLParams params(
      signin::GetAddAccountURLForDice("", GURL()), content::Referrer(),
      WindowOpenDisposition::NEW_POPUP, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);
  add_account_popup_ = source_window->GetDelegate()->OpenURLFromTab(
      source_window, params, /*navigation_handle_callback=*/{});
  ResizeAndFocusAddAccountPopup();
  add_account_popup_observer_->ObservePopup(add_account_popup_);
}

void AccountChooserController::OnAddAccountButtonClicked() {
  ShowAddAccountDialog();
}

void AccountChooserController::OnFlowCancelled() {
  if (on_account_chosen_callback_) {
    scoped_identity_manager_observation_.Reset();
    CloseDialogs();
    std::move(on_account_chosen_callback_).Run(std::nullopt);
  }
}

void AccountChooserController::OnAccountSelected(
    const AccountInfo& account_info) {
  selected_account_ = account_info;
}

void AccountChooserController::OnSaveButtonClicked() {
  if (on_account_chosen_callback_) {
    CHECK(selected_account_.has_value());
    scoped_identity_manager_observation_.Reset();
    CloseDialogs();
    std::move(on_account_chosen_callback_).Run(selected_account_);
  }
}

AccountChooserController::ProfileInfo
AccountChooserController::GetProfileInfo() {
  ProfileInfo profile_info;
  profile_info.accounts =
      identity_manager_->GetExtendedAccountInfoForAccountsWithRefreshToken();
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    profile_info.primary_account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  }
  return profile_info;
}

void AccountChooserController::CloseWidget() {
  if (account_chooser_widget_) {
    // Must be set to nullptr before the widget is destroyed to avoid dangling
    // ptr.
    account_chooser_view_ = nullptr;
    account_chooser_widget_.reset();
    account_chooser_dialog_delegate_.reset();
  }
}

std::unique_ptr<views::DialogDelegate>
AccountChooserController::CreateDialogDelegate(
    std::unique_ptr<AccountChooserView> account_chooser_view) {
  auto dialog_delegate = std::make_unique<views::DialogDelegate>();
  dialog_delegate->SetModalType(ui::mojom::ModalType::kChild);
  dialog_delegate->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  dialog_delegate->set_fixed_width(kDialogWidth);
  int dialog_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW);
  dialog_delegate->set_margins(gfx::Insets::TLBR(dialog_margin, dialog_margin,
                                                 dialog_margin, dialog_margin));
  dialog_delegate->SetShowTitle(false);
  dialog_delegate->SetShowCloseButton(false);
  dialog_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  dialog_delegate->SetContentsView(std::move(account_chooser_view));
  return dialog_delegate;
}

void AccountChooserController::ResizeAndFocusAddAccountPopup() {
  CHECK(add_account_popup_);
  gfx::Rect popup_window_bounds = ComputePopupWindowBounds(tab_->GetContents());
  add_account_popup_->GetDelegate()->SetContentsBounds(add_account_popup_,
                                                       popup_window_bounds);
  add_account_popup_->GetDelegate()->ActivateContents(add_account_popup_);
}

void AccountChooserController::CloseAddAccountPopup() {
  if (!add_account_popup_) {
    return;
  }
  add_account_popup_programatically_closed_ = true;
  // Store this in a local variable to avoid triggering the dangling pointer
  // detector.
  content::WebContents* popup = add_account_popup_;
  add_account_popup_ = nullptr;
  popup->Close();
}

void AccountChooserController::CloseDialogs() {
  CloseAddAccountPopup();
  CloseWidget();
}

}  // namespace save_to_drive
