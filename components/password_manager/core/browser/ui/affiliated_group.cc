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

}  // namespace password_manager
