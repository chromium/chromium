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
#include "ui/views/window/dialog_delegate.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace content {
class WebContents;
}  // namespace content

namespace save_to_drive {
class AccountChooserView;

using AccountChosenCallback =
    base::OnceCallback<void(std::optional<AccountInfo>)>;

// This class is responsible for showing the flow for selecting an account to
// save to Drive.
class AccountChooserController : public signin::IdentityManager::Observer,
                                 public AccountChooserViewDelegate {
 public:
  AccountChooserController(content::WebContents* web_contents,
                           signin::IdentityManager* identity_manager);
  AccountChooserController(const AccountChooserController&) = delete;
  AccountChooserController& operator=(const AccountChooserController&) = delete;
  ~AccountChooserController() override;

  void GetAccount(AccountChosenCallback on_account_chosen_callback);

  // IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

 private:
  // Stores account information for a profile.
  struct ProfileInfo {
    ProfileInfo();
    ProfileInfo(const ProfileInfo&);
    ProfileInfo& operator=(const ProfileInfo&);
    ~ProfileInfo();
    // Accounts with refresh tokens.
    std::vector<AccountInfo> accounts;
    // Primary account id, if it exists.
    std::optional<CoreAccountId> primary_account_id = std::nullopt;
  };

  // Observes the add account popup window's WebContents.
  class AddAccountPopupObserver;

  // If no accounts are present, shows the add account dialog. Otherwise,
  // shows the account chooser dialog.
  void Show();
  // Shows the account chooser dialog.
  void ShowAccountChooserDialog(ProfileInfo profile_info);
  // Shows the add account dialog.
  void ShowAddAccountDialog();

  // AccountChooserViewDelegate:
  void OnAddAccountButtonClicked() override;
  void OnFlowCancelled() override;
  void OnAccountSelected(const AccountInfo& account_info) override;
  void OnSaveButtonClicked() override;

  // Gets accounts associated with the profile.
  ProfileInfo GetProfileInfo();
  // Cleans up the account chooser widget.
  void CloseWidget();
  // Resizes and focuses the add account popup.
  void ResizeAndFocusAddAccountPopup();
  // Closes the add account popup.
  void CloseAddAccountPopup();
  // Closes all dialogs.
  void CloseDialogs();
  // Called when the add account popup is destroyed.
  void OnAddAccountPopupDestroyed();
  // Creates a dialog delegate for the account chooser dialog.
  std::unique_ptr<views::DialogDelegate> CreateDialogDelegate(
      std::unique_ptr<AccountChooserView> account_chooser_view);
  // Called when the flow is cancelled via widget action (like escape key).
  void OnWidgetCancelledFlow(views::Widget::ClosedReason reason);

  raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  // All dialogs MUST be closed if this callback has been run.
  AccountChosenCallback on_account_chosen_callback_;

  // Account chooser dialog related fields.
  raw_ptr<AccountChooserView> account_chooser_view_;
  std::unique_ptr<views::DialogDelegate> account_chooser_dialog_delegate_;
  std::unique_ptr<views::Widget> account_chooser_widget_;
  std::optional<AccountInfo> selected_account_;

  // Add account popup related fields.
  raw_ptr<content::WebContents> add_account_popup_;
  std::unique_ptr<AddAccountPopupObserver> add_account_popup_observer_;
  bool add_account_popup_programatically_closed_ = false;
};
}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_H_
