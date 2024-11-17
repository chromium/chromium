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
    webauthn::PasskeyModel* passkey_model)
    : passkey_model_(passkey_model) {
  // Immediate observation of the passkeys model is essential to signal the
  // source adapter upon model destruction. This prevents dangling pointers by
  // automatically removing observation and resetting the model pointer.
  passkey_model_observation_.Observe(passkey_model_);
}

PasskeyAffiliationSourceAdapter::~PasskeyAffiliationSourceAdapter() {
  passkey_model_observation_.Reset();
  passkey_model_ = nullptr;
}

void PasskeyAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  // This can happen in tests.
  if (!passkey_model_) {
    std::move(response_callback).Run({});
    return;
  }

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

void PasskeyAffiliationSourceAdapter::StartObserving(
    AffiliationSource::Observer* observer) {
  CHECK(!observer_);
  // Note that the data layer is already being observed. Only setting up the
  // correct affiliation source observer is necessary.
  observer_ = observer;
}

void PasskeyAffiliationSourceAdapter::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  if (!observer_) {
    return;
  }
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
  passkey_model_ = nullptr;
}

void PasskeyAffiliationSourceAdapter::OnPasskeyModelIsReady(bool is_ready) {}

}  // namespace password_manager
