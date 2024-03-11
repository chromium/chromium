// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace password_manager {
namespace {
using affiliations::FacetURI;

std::optional<FacetURI> FacetURIFromPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  const std::string as_url = base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()});
  FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(as_url);
  if (!facet_uri.is_valid()) {
    return std::nullopt;
  }
  if (!facet_uri.IsValidAndroidFacetURI() && !facet_uri.IsValidWebFacetURI()) {
    return std::nullopt;
  }
  return facet_uri;
}

}  // namespace

PasskeyAffiliationSourceAdapter::PasskeyAffiliationSourceAdapter(
    webauthn::PasskeyModel* passkey_model,
    AffiliationSource::Observer* observer)
    : passkey_model_(passkey_model), observer_(*observer) {}

PasskeyAffiliationSourceAdapter::~PasskeyAffiliationSourceAdapter() = default;

void PasskeyAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetAllPasskeys();
  std::vector<FacetURI> result;
  result.reserve(passkeys.size());
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    if (std::optional<FacetURI> facet = FacetURIFromPasskey(passkey)) {
      result.push_back(std::move(*facet));
    }
  }

  std::move(response_callback).Run(std::move(result));
}

void PasskeyAffiliationSourceAdapter::StartObserving() {
  passkey_model_observation_.Observe(passkey_model_);
}

void PasskeyAffiliationSourceAdapter::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  std::vector<FacetURI> facets_added;
  std::vector<FacetURI> facets_removed;

  for (const webauthn::PasskeyModelChange& change : changes) {
    if (std::optional<FacetURI> facet = FacetURIFromPasskey(change.passkey())) {
      if (change.type() == webauthn::PasskeyModelChange::ChangeType::ADD) {
        facets_added.push_back(std::move(*facet));
      } else if (change.type() ==
                 webauthn::PasskeyModelChange::ChangeType::REMOVE) {
        facets_removed.push_back(std::move(*facet));
      }
    }
  }

  if (!facets_added.empty()) {
    observer_->OnFacetsAdded(std::move(facets_added));
  }
  if (!facets_removed.empty()) {
    observer_->OnFacetsRemoved(std::move(facets_removed));
  }
}

void PasskeyAffiliationSourceAdapter::OnPasskeyModelShuttingDown() {
  passkey_model_observation_.Reset();
}

}  // namespace password_manager
