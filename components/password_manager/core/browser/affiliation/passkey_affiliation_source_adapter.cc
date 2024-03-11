// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/webauthn/core/browser/passkey_model.h"

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
  // TODO(b/328037758): Observe passkey changes.
}

}  // namespace password_manager
