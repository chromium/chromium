// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

#include <algorithm>

namespace password_manager {

AffiliatedGroup::AffiliatedGroup() = default;

AffiliatedGroup::AffiliatedGroup(
    const std::vector<CredentialUIEntry> credential_groups)
    : credential_groups_(std::move(credential_groups)) {}

AffiliatedGroup::AffiliatedGroup(const AffiliatedGroup& other) = default;
AffiliatedGroup::AffiliatedGroup(AffiliatedGroup&& other) = default;

AffiliatedGroup::~AffiliatedGroup() = default;

AffiliatedGroup& AffiliatedGroup::operator=(const AffiliatedGroup& other) =
    default;

AffiliatedGroup& AffiliatedGroup::operator=(AffiliatedGroup&& other) = default;

void AffiliatedGroup::AddCredential(const CredentialUIEntry& credential) {
  credential_groups_.push_back(credential);
}

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
  if (lhs.GetCredentialGroups().size() != rhs.GetCredentialGroups().size()) {
    return false;
  }

  // Sort credential groups vectors.
  std::vector<CredentialUIEntry> lhs_credential_groups =
      lhs.GetCredentialGroups();
  std::sort(lhs_credential_groups.begin(), lhs_credential_groups.end());

  std::vector<CredentialUIEntry> rhs_credential_groups =
      rhs.GetCredentialGroups();
  std::sort(rhs_credential_groups.begin(), rhs_credential_groups.end());

  if (!base::ranges::equal(lhs_credential_groups, rhs_credential_groups)) {
    return false;
  }
  return true;
}

}  // namespace password_manager
