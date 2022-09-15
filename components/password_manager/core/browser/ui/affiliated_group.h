// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Struct that represents a collection of credential groups that are grouped
// based on an Affiliation.
struct AffiliatedGroup {
  AffiliatedGroup();
  explicit AffiliatedGroup(
      const std::vector<CredentialUIEntry> credential_groups);
  AffiliatedGroup(const AffiliatedGroup& other);
  AffiliatedGroup(AffiliatedGroup&& other);
  ~AffiliatedGroup();

  // The branding information for the affiliated group. Corresponds to the
  // `BrandingInfo` message in affiliation_api.proto.
  FacetBrandingInfo branding_info;

  // List of credential groups in the affiliated group.
  std::vector<CredentialUIEntry> credential_groups;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
