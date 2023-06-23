// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_

#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Struct that represents a collection of credential groups that are grouped
// based on an Affiliation.
class AffiliatedGroup {
 public:
  AffiliatedGroup();
  AffiliatedGroup(std::vector<CredentialUIEntry> credentials,
                  const FacetBrandingInfo& branding);
  AffiliatedGroup(const AffiliatedGroup& other);
  AffiliatedGroup(AffiliatedGroup&& other);
  AffiliatedGroup& operator=(const AffiliatedGroup& other);
  AffiliatedGroup& operator=(AffiliatedGroup&& other);
  ~AffiliatedGroup();

  // Credential Groups Getter.
  base::span<const CredentialUIEntry> GetCredentials() const {
    return base::make_span(credential_groups_.begin(),
                           credential_groups_.end());
  }

  // Method that returns the display name for this affiliated group.
  const std::string& GetDisplayName() const { return branding_info_.name; }

  // Method that returns the icon URL for this affiliated group.
  const GURL& GetIconURL() const { return branding_info_.icon_url; }

  // Fallback icon when icon returned by the affiliation service can't be used.
  GURL GetFallbackIconURL() const;

 private:
  // The branding information for the affiliated group. Corresponds to the
  // `BrandingInfo` message in affiliation_api.proto.
  FacetBrandingInfo branding_info_;

  // List of credential groups in the affiliated group.
  std::vector<CredentialUIEntry> credential_groups_;
};

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
