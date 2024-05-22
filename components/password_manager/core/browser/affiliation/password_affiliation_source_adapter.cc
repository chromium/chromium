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

PasswordAffiliationSourceAdapter::PasswordAffiliationSourceAdapter() = default;
PasswordAffiliationSourceAdapter::~PasswordAffiliationSourceAdapter() = default;

void PasswordAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  if (is_fetching_canceled_) {
    std::move(response_callback).Run({});
    return;
  }

  on_password_forms_received_callback_ = std::move(response_callback);
  store_->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordAffiliationSourceAdapter::StartObserving(
    AffiliationSource::Observer* observer) {
  CHECK(!observer_);
  observer_ = observer;
  scoped_observation_.Observe(store_);
}

void PasswordAffiliationSourceAdapter::RegisterPasswordStore(
    PasswordStoreInterface* store) {
  store_ = store;
}

void PasswordAffiliationSourceAdapter::DisableSource() {
  // Don't do anything if fetching was canceled already.
  if (is_fetching_canceled_) {
    return;
  }

  is_fetching_canceled_ = true;
  scoped_observation_.Reset();
}

void PasswordAffiliationSourceAdapter::OnLoginsChanged(
    PasswordStoreInterface* /*store*/,
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facets_added;
  std::vector<FacetURI> facets_removed;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(change.form().signon_realm);

    if (!facet_uri.is_valid()) {
      continue;
    }

    if (!IsFacetValidForAffiliation(facet_uri)) {
      continue;
    }

    if (change.type() == PasswordStoreChange::ADD) {
      facets_added.push_back(std::move(facet_uri));
    } else if (change.type() == PasswordStoreChange::REMOVE) {
      facets_removed.push_back(std::move(facet_uri));
    }
  }
  // When the primary key for a login is updated, `changes` will contain both a
  // REMOVE and ADD change for that login. Cached affiliation data should not be
  // deleted in this case. A simple solution is to call `added` events always
  // before `removed` -- the trimming logic will detect that there is an active
  // prefetch and not delete the corresponding data.
  if (!facets_added.empty()) {
    observer_->OnFacetsAdded(std::move(facets_added));
  }
  if (!facets_removed.empty()) {
    observer_->OnFacetsRemoved(std::move(facets_removed));
  }
}

void PasswordAffiliationSourceAdapter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  // TODO(b/328037758): Handle retained passwords.
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
