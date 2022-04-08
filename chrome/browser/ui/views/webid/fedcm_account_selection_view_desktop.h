// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include "chrome/browser/ui/webid/account_selection_view.h"

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
class FedCmAccountSelectionView : public AccountSelectionView {
 public:
  explicit FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  void Show(const std::string& rp_etld_plus_one,
            const std::string& idp_etld_plus_one,
            base::span<const Account> accounts,
            const content::IdentityProviderMetadata& idp_metadata,
            const content::ClientIdData& client_data,
            Account::SignInMode sign_in_mode) override;

 private:
  base::WeakPtr<views::Widget> bubble_widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
