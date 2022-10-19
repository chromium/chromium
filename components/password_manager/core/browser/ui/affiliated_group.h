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
class AffiliatedGroup {
 public:
  AffiliatedGroup();
  explicit AffiliatedGroup(
      const std::vector<CredentialUIEntry> credential_groups);
  AffiliatedGroup(const AffiliatedGroup& other);
  AffiliatedGroup(AffiliatedGroup&& other);
  AffiliatedGroup& operator=(const AffiliatedGroup& other);
  AffiliatedGroup& operator=(AffiliatedGroup&& other);
  ~AffiliatedGroup();

  // Method to add a credential to the credential group.
  void AddCredential(const CredentialUIEntry& credential);

  // Credential Groups Getter.
  const std::vector<CredentialUIEntry>& GetCredentialGroups() const {
    return credential_groups_;
  }

  // Branding Info Setter.
  void SetBrandingInfo(const FacetBrandingInfo& branding_info) {
    branding_info_ = branding_info;
  }

  // Branding Info Getter.
  const FacetBrandingInfo& GetBrandingInfo() const { return branding_info_; }

 private:
  // The branding information for the affiliated group. Corresponds to the
  // `BrandingInfo` message in affiliation_api.proto.
  FacetBrandingInfo branding_info_;

  // List of credential groups in the affiliated group.
  std::vector<CredentialUIEntry> credential_groups_;
};

#ifdef UNIT_TEST
bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs);
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
