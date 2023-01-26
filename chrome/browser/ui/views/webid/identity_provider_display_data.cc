// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"

IdentityProviderDisplayData::IdentityProviderDisplayData(
    const std::u16string& idp_etld_plus_one,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientMetadata& client_metadata,
    const std::vector<content::IdentityRequestAccount>& accounts)
    : idp_etld_plus_one(idp_etld_plus_one),
      idp_metadata(idp_metadata),
      client_metadata(client_metadata),
      accounts(accounts) {}

IdentityProviderDisplayData::IdentityProviderDisplayData(
    const IdentityProviderDisplayData& other) = default;

IdentityProviderDisplayData::~IdentityProviderDisplayData() = default;
