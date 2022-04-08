// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
#define CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/strong_alias.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "url/gurl.h"

using Account = content::IdentityRequestAccount;

// This class represents the interface used for communicating between the
// identity dialog controller with the Android frontend.
class AccountSelectionView {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Informs the controller that the user has made a selection.
    virtual void OnAccountSelected(const Account& account) = 0;
    // Informs the controller that the user has dismissed the sheet.
    virtual void OnDismiss() = 0;
    // The web page view containing the focused field.
    virtual gfx::NativeView GetNativeView() = 0;
    // The WebContents for the page.
    virtual content::WebContents* GetWebContents() = 0;
  };

  static std::unique_ptr<AccountSelectionView> Create(Delegate* delegate);
  static int GetBrandIconMinimumSize();
  static int GetBrandIconIdealSize();

  explicit AccountSelectionView(Delegate* delegate) : delegate_(delegate) {}
  AccountSelectionView(const AccountSelectionView&) = delete;
  AccountSelectionView& operator=(const AccountSelectionView&) = delete;
  virtual ~AccountSelectionView() = default;

  // Instructs the view to show the provided |accounts| to the user.
  // |rp_etld_plus_one| is the relying party's ETLD+1 which is normally the
  // current page's domain, and |idp_etld_plus_one| is the identity provider's
  // ETLD+1 that is providing the accounts. |sign_in_mode| represents whether
  // this is an auto sign in flow. After user interaction either
  // OnAccountSelected() or OnDismiss() gets invoked.
  virtual void Show(const std::string& rp_etld_plus_one,
                    const std::string& idp_etld_plus_one,
                    base::span<const Account> accounts,
                    const content::IdentityProviderMetadata& idp_metadata,
                    const content::ClientIdData& client_data,
                    Account::SignInMode sign_in_mode) = 0;

 protected:
  raw_ptr<Delegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
