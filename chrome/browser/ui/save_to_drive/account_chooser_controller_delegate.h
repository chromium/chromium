// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_DELEGATE_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace save_to_drive {

class AccountChooserControllerDelegate {
 public:
  AccountChooserControllerDelegate() = default;
  AccountChooserControllerDelegate(const AccountChooserControllerDelegate&) =
      delete;
  AccountChooserControllerDelegate& operator=(
      const AccountChooserControllerDelegate&) = delete;
  virtual ~AccountChooserControllerDelegate() = default;

  // Closes the add account dialog.
  virtual void CloseAddAccountDialog() = 0;
  // Shows the add account dialog.
  virtual void ShowAddAccountDialog() = 0;

  // Called when an account is selected.
  virtual void OnAccountSelected(const AccountInfo& account) = 0;
  // Called when the flow is cancelled.
  virtual void OnFlowCancelled() = 0;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_CONTROLLER_DELEGATE_H_
