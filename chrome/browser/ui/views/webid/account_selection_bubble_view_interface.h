// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_

#include <string>

#include "base/containers/span.h"

namespace content {
struct ClientIdData;
struct IdentityProviderMetadata;
struct IdentityRequestAccount;
}  // namespace content

// Interface for interacting with FedCM account selection bubble.
class AccountSelectionBubbleViewInterface {
 public:
  virtual ~AccountSelectionBubbleViewInterface() = default;

  // Updates the FedCM bubble to show the "account picker" sheet.
  virtual void ShowAccountPicker(
      const std::u16string& idp_for_display,
      bool show_back_button,
      base::span<const content::IdentityRequestAccount> accounts,
      const content::IdentityProviderMetadata& idp_metadata,
      const content::ClientIdData& client_data) = 0;

  // Updates the FedCM bubble to show the "verifying" sheet.
  virtual void ShowVerifyingSheet(
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderMetadata& idp_metadata) = 0;

  // Updates the FedCM bubble to show the "failure" sheet.
  virtual void ShowFailureDialog(const std::u16string& rp_for_display,
                                 const std::u16string& idp_for_display) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_
