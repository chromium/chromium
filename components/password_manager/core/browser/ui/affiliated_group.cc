// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

#include <algorithm>

#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager {

namespace {

constexpr char kFavicon[] = "favicon.ico";

// TODO(crbug.com/40066949): Remove this method. This is a duplicate of
// password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords but this
// file cannot include the sync_util as the sync util is part of the whole
// browser target.
bool IsSyncFeatureEnabledIncludingPasswords(
    const syncer::SyncService* sync_service) {
  return sync_service && sync_service->IsSyncFeatureEnabled() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

}  // namespace

AffiliatedGroup::AffiliatedGroup() = default;
AffiliatedGroup::AffiliatedGroup(
    std::vector<CredentialUIEntry> credentials,
    const affiliations::FacetBrandingInfo& branding)
    : branding_info_(branding), credential_groups_(std::move(credentials)) {}
AffiliatedGroup::AffiliatedGroup(const AffiliatedGroup& other) = default;
AffiliatedGroup::AffiliatedGroup(AffiliatedGroup&& other) = default;

AffiliatedGroup::~AffiliatedGroup() = default;

AffiliatedGroup& AffiliatedGroup::operator=(const AffiliatedGroup& other) =
    default;
AffiliatedGroup& AffiliatedGroup::operator=(AffiliatedGroup&& other) = default;

GURL AffiliatedGroup::GetFallbackIconURL() const {
  for (const auto& credential : credential_groups_) {
    for (const auto& facet : credential.facets) {
      // Ignore non https schemes.
      if (facet.url.SchemeIs(url::kHttpsScheme)) {
        GURL::Replacements replacements;
        replacements.SetPathStr(kFavicon);
        return facet.url.GetWithEmptyPath().ReplaceComponents(replacements);
      }
    }
  }
  return GURL();
}

GURL AffiliatedGroup::GetAllowedIconUrl(
    const syncer::SyncService* sync_service) const {
  if (!sync_service) {
    return GetFallbackIconURL();
  }

  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    // Users with explicit passphrase should only use fallback icon.
    return GetFallbackIconURL();
  }

  // TODO(crbug.com/40066949): Remove this codepath once
  // `IsSyncFeatureEnabled()` is fully deprecated.
  if (IsSyncFeatureEnabledIncludingPasswords(sync_service)) {
    // Syncing users can use icon provided by the affiliation service.
    return GetIconURL();
  }

  for (const password_manager::CredentialUIEntry& credential :
       GetCredentials()) {
    if (credential.stored_in.contains(
            password_manager::PasswordForm::Store::kAccountStore)) {
      // If at least one credential is stored in the account, icon provided by
      // the affiliation service can be used for the whole group.
      return GetIconURL();
    }
  }

  return GetFallbackIconURL();
}

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
  if (!std::ranges::equal(lhs.GetCredentials(), rhs.GetCredentials())) {
    return false;
  }
  return lhs.GetDisplayName() == rhs.GetDisplayName() &&
         lhs.GetIconURL() == rhs.GetIconURL();
}

}  // namespace password_manager
