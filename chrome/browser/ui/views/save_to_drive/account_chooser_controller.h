// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace save_to_drive {
class AccountChooserView;

// This class is responsible for showing the flow for selecting an account to
// save to Drive.
class AccountChooserController
    : public content::WebContentsObserver,  // observes
                                            // add_account_popup
      public signin::IdentityManager::Observer,
      // ties lifetime to WebContents
      public content::WebContentsUserData<AccountChooserController>,
      public AccountChooserViewDelegate {
 public:
  AccountChooserController(tabs::TabInterface* tab,
                           signin::IdentityManager* identity_manager);
  AccountChooserController(const AccountChooserController&) = delete;
  AccountChooserController& operator=(const AccountChooserController&) = delete;
  ~AccountChooserController() override;

  void GetAccount(base::OnceCallback<void(std::optional<AccountInfo>)>
                      on_account_selected_callback);

  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  // If no accounts are present, shows the add account dialog. Otherwise,
  // shows the account chooser dialog.
  void Show();
  // Shows the account chooser dialog.
  void ShowAccountChooserDialog();
  // Shows the add account dialog.
  void ShowAddAccountDialog();
  // User clicked "Use different account" button in the account chooser dialog.
  void OnAddAccountButtonClicked() override;
  // User clicked "cancel" or closed the add account pop-up with no accounts
  // present
  void OnFlowCancelled(int32_t widget_closed_reason) override;
  // User clicked on an account
  void OnAccountSelected(const AccountInfo& account_info) override;
  // User clicked "Save" button in the account chooser dialog.
  void OnSaveButtonClicked() override;

  // tabs::TabInterface* tab_;
  // signin::IdentityManager* identity_manager_;
  // base::ScopedObservation<signin::IdentityManager,
  //                         signin::IdentityManager::Observer>
  //     scoped_identity_manager_observation_{this};
  // base::OnceCallback<void(std::optional<AccountInfo>)>
  //     on_account_selected_callback_;
  // std::vector<AccountInfo> accounts_;
  // raw_ptr<AccountChooserView> account_chooser_view_;
  // // created using tab_dialog_manager.h
  // std::unique_ptr<views::Widget> account_chooser_widget_;
  // std::optional<AccountInfo> selected_account_;
  // raw_ptr<content::WebContents> add_account_popup_;
};
}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_H_
