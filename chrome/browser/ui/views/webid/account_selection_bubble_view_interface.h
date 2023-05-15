// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"

namespace content {
struct IdentityRequestAccount;
}  // namespace content

// Interface for interacting with FedCM account selection bubble.
class AccountSelectionBubbleViewInterface {
 public:
  virtual ~AccountSelectionBubbleViewInterface() = default;

  // Updates the FedCM bubble to show the "account picker" sheet.
  virtual void ShowMultiAccountPicker(
      const std::vector<IdentityProviderDisplayData>& idp_data_list) = 0;

  // Updates the FedCM bubble to show the "verifying" sheet.
  virtual void ShowVerifyingSheet(
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_data,
      const std::u16string& title) = 0;

  // Updates to show single account plus a confirm dialog. Used when showing the
  // account confirmation dialog after the user picks one of multiple accounts.
  virtual void ShowSingleAccountConfirmDialog(
      const std::u16string& top_frame_for_display,
      const absl::optional<std::u16string>& iframe_for_display,
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_data,
      bool show_back_button) = 0;

  // Updates the FedCM bubble to show the "failure" sheet.
  virtual void ShowFailureDialog(
      const std::u16string& top_frame_for_display,
      const absl::optional<std::u16string>& iframe_for_display,
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) = 0;

  virtual std::string GetDialogTitle() const = 0;
  virtual absl::optional<std::string> GetDialogSubtitle() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_INTERFACE_H_
