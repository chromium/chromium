// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

// Struct that represents a collection of credential groups that are grouped
// based on an Affiliation.
class AffiliatedGroup final {
 public:
  AffiliatedGroup();
  AffiliatedGroup(std::vector<CredentialUIEntry> credentials,
                  const affiliations::FacetBrandingInfo& branding);
  AffiliatedGroup(const AffiliatedGroup& other);
  AffiliatedGroup(AffiliatedGroup&& other);
  AffiliatedGroup& operator=(const AffiliatedGroup& other);
  AffiliatedGroup& operator=(AffiliatedGroup&& other);
  ~AffiliatedGroup();

  // Credential Groups Getter.
  base::span<const CredentialUIEntry> GetCredentials() const {
    return credential_groups_;
  }

  // Method that returns the display name for this affiliated group.
  const std::string& GetDisplayName() const { return branding_info_.name; }

  // Fallback icon when icon returned by the affiliation service can't be used.
  GURL GetFallbackIconURL() const;

  // Choose between fallback and main icon based on if the user allowed syncing.
  GURL GetAllowedIconUrl(const syncer::SyncService* sync_service) const;

  friend bool operator==(const AffiliatedGroup& lhs,
                         const AffiliatedGroup& rhs);

 private:
  // Method that returns the icon URL for this affiliated group.
  const GURL& GetIconURL() const { return branding_info_.icon_url; }

  // The branding information for the affiliated group. Corresponds to the
  // `BrandingInfo` message in affiliation_api.proto.
  affiliations::FacetBrandingInfo branding_info_;

  // List of credential groups in the affiliated group.
  std::vector<CredentialUIEntry> credential_groups_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_AFFILIATED_GROUP_H_
