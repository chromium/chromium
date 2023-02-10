// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

#include <algorithm>

namespace password_manager {

AffiliatedGroup::AffiliatedGroup() = default;
AffiliatedGroup::AffiliatedGroup(std::vector<CredentialUIEntry> credentials,
                                 const FacetBrandingInfo& branding)
    : branding_info_(branding), credential_groups_(std::move(credentials)) {}
AffiliatedGroup::AffiliatedGroup(const AffiliatedGroup& other) = default;
AffiliatedGroup::AffiliatedGroup(AffiliatedGroup&& other) = default;

AffiliatedGroup::~AffiliatedGroup() = default;

AffiliatedGroup& AffiliatedGroup::operator=(const AffiliatedGroup& other) =
    default;
AffiliatedGroup& AffiliatedGroup::operator=(AffiliatedGroup&& other) = default;

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
  if (!base::ranges::equal(lhs.GetCredentials(), rhs.GetCredentials())) {
    return false;
  }
  return lhs.GetDisplayName() == rhs.GetDisplayName() &&
         lhs.GetIconURL() == rhs.GetIconURL();
}

}  // namespace password_manager
