// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

#include <algorithm>

namespace password_manager {

namespace {

constexpr char kFavicon[] = "favicon.ico";

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

bool operator==(const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
  if (!base::ranges::equal(lhs.GetCredentials(), rhs.GetCredentials())) {
    return false;
  }
  return lhs.GetDisplayName() == rhs.GetDisplayName() &&
         lhs.GetIconURL() == rhs.GetIconURL();
}

}  // namespace password_manager
