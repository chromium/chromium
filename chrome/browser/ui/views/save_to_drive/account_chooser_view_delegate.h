// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace save_to_drive {

class AccountChooserViewDelegate {
 public:
  AccountChooserViewDelegate() = default;
  AccountChooserViewDelegate(const AccountChooserViewDelegate&) = delete;
  AccountChooserViewDelegate& operator=(const AccountChooserViewDelegate&) =
      delete;
  virtual ~AccountChooserViewDelegate() = default;

  // Persists state changes when a user clicks on an account.
  virtual void OnAccountSelected(const AccountInfo& account) = 0;
  // Called when the user clicks on the "Use a different account" button.
  virtual void OnAddAccountButtonClicked() = 0;
  // Called when the dialog is closed or cancelled.
  virtual void OnFlowCancelled() = 0;
  // Called when the save button is clicked.
  virtual void OnSaveButtonClicked() = 0;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_
