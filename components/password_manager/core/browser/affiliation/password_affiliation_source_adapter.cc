// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;

// Filling across affiliated sites is implemented differently on Android.
bool IsFacetValidForAffiliation(const FacetURI& facet) {
#if BUILDFLAG(IS_ANDROID)
  return facet.IsValidAndroidFacetURI();
#else
  return facet.IsValidAndroidFacetURI() || facet.IsValidWebFacetURI();
#endif
}
}  // namespace

PasswordAffiliationSourceAdapter::PasswordAffiliationSourceAdapter(
    PasswordStoreInterface* store,
    AffiliationSource::Observer* observer)
    : store_(*store), observer_(*observer) {}

PasswordAffiliationSourceAdapter::~PasswordAffiliationSourceAdapter() = default;

void PasswordAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  on_password_forms_received_callback_ = std::move(response_callback);
  store_->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordAffiliationSourceAdapter::StartObserving() {
  // TODO(b/328037758): Implement PasswordStoreInterface::Observer.
}

void PasswordAffiliationSourceAdapter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  std::vector<FacetURI> facets;
  for (const std::unique_ptr<PasswordForm>& form : results) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm);
    if (IsFacetValidForAffiliation(facet_uri)) {
      facets.push_back(std::move(facet_uri));
    }
  }
  std::move(on_password_forms_received_callback_).Run(std::move(facets));
}

}  // namespace password_manager
