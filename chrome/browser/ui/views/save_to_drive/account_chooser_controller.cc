// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"

#include "components/tabs/public/tab_interface.h"

namespace save_to_drive {
AccountChooserController::AccountChooserController(
    tabs::TabInterface* tab,
    signin::IdentityManager* identity_manager)
    : WebContentsUserData<AccountChooserController>(*tab->GetContents()) {}
AccountChooserController::~AccountChooserController() = default;

void AccountChooserController::GetAccount(
    base::OnceCallback<void(std::optional<AccountInfo>)>
        on_account_selected_callback) {}
void AccountChooserController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {}
void AccountChooserController::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {}
void AccountChooserController::WebContentsDestroyed() {}
void AccountChooserController::Show() {}
void AccountChooserController::ShowAccountChooserDialog() {}
void AccountChooserController::ShowAddAccountDialog() {}
void AccountChooserController::OnAddAccountButtonClicked() {}
void AccountChooserController::OnFlowCancelled(int32_t widget_closed_reason) {}
void AccountChooserController::OnAccountSelected(
    const AccountInfo& account_info) {}
void AccountChooserController::OnSaveButtonClicked() {}
}  // namespace save_to_drive
