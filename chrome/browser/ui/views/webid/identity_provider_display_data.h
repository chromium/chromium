// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_IDENTITY_PROVIDER_DISPLAY_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_IDENTITY_PROVIDER_DISPLAY_DATA_H_

#include <string>

#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"

struct IdentityProviderDisplayData {
  IdentityProviderDisplayData(
      const std::u16string& idp_etld_plus_one,
      const content::IdentityProviderMetadata& idp_metadata,
      const content::ClientMetadata& client_metadata,
      const std::vector<content::IdentityRequestAccount>& accounts,
      bool request_permission);

  IdentityProviderDisplayData(const IdentityProviderDisplayData& other);

  ~IdentityProviderDisplayData();

  std::u16string idp_etld_plus_one;
  content::IdentityProviderMetadata idp_metadata;
  content::ClientMetadata client_metadata;
  std::vector<content::IdentityRequestAccount> accounts;
  bool request_permission;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_IDENTITY_PROVIDER_DISPLAY_DATA_H_
