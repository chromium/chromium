// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

namespace password_manager {

AffiliatedGroup::AffiliatedGroup() = default;

AffiliatedGroup::AffiliatedGroup(
    const std::vector<CredentialUIEntry> credential_groups)
    : credential_groups(std::move(credential_groups)) {}

AffiliatedGroup::AffiliatedGroup(const AffiliatedGroup& other) = default;
AffiliatedGroup::AffiliatedGroup(AffiliatedGroup&& other) = default;

AffiliatedGroup::~AffiliatedGroup() = default;

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
  if (lhs.credential_groups.size() != rhs.credential_groups.size()) {
    return false;
  }
  for (const CredentialUIEntry& credential : lhs.credential_groups) {
    if (std::find(rhs.credential_groups.begin(), rhs.credential_groups.end(),
                  credential) == rhs.credential_groups.end()) {
      return false;
    }
  }
  return true;
}

}  // namespace password_manager
