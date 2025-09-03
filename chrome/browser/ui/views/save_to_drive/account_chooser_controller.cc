// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_util.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"
#include "components/tabs/public/tab_interface.h"

namespace save_to_drive {

AccountChooserController::ProfileInfo::ProfileInfo() = default;
AccountChooserController::ProfileInfo::ProfileInfo(const ProfileInfo&) =
    default;
AccountChooserController::ProfileInfo&
AccountChooserController::ProfileInfo::operator=(const ProfileInfo&) = default;
AccountChooserController::ProfileInfo::~ProfileInfo() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(AccountChooserController);

AccountChooserController::AccountChooserController(
    content::WebContents* web_contents,
    signin::IdentityManager* identity_manager)
    : WebContentsUserData<AccountChooserController>(*web_contents),
      tab_(tabs::TabInterface::MaybeGetFromContents(web_contents)),
      identity_manager_(identity_manager) {}

AccountChooserController::~AccountChooserController() {
  CloseWidget();
}

void AccountChooserController::GetAccount(
    base::OnceCallback<void(std::optional<AccountInfo>)>
        on_account_selected_callback) {
  ProfileInfo profile_info = GetProfileInfo();
  Show(std::move(profile_info));
}

void AccountChooserController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {}
void AccountChooserController::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {}

void AccountChooserController::Show(ProfileInfo profile_info) {
  if (profile_info.accounts.empty()) {
    // TODO: show add account dialog
  } else {
    ShowAccountChooserDialog(std::move(profile_info));
  }
}

void AccountChooserController::ShowAccountChooserDialog(
    ProfileInfo profile_info) {
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

void AccountChooserController::ShowAddAccountDialog() {}
void AccountChooserController::OnAddAccountButtonClicked() {}
void AccountChooserController::OnFlowCancelled(int32_t widget_closed_reason) {}
void AccountChooserController::OnAccountSelected(
    const AccountInfo& account_info) {}
void AccountChooserController::OnSaveButtonClicked() {}

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

}  // namespace save_to_drive
