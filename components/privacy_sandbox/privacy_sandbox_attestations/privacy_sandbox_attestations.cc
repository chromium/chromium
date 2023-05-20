// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "base/containers/contains.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

PrivacySandboxAttestations::PrivacySandboxAttestations(
    const PrivacySandboxAttestationsMap& attestations_map)
    : attestations_map_(attestations_map) {}
PrivacySandboxAttestations::~PrivacySandboxAttestations() = default;

bool PrivacySandboxAttestations::IsSiteAttested(
    net::SchemefulSite site,
    PrivacySandboxAttestationsGatedAPI invoking_api) const {
  // If attestations aren't enabled, pass the check trivially.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    return true;
  }

  // Pass the check if the site is in the list of devtools overrides.
  if (base::Contains(overrides_, site)) {
    return true;
  }

  // If `site` isn't enrolled at all, fail the check.
  auto it = attestations_map_.find(site);
  if (it == attestations_map_.end()) {
    return false;
  }

  // If `site` is attested for `invoking_api`, pass the check.
  if (it->second.Has(invoking_api)) {
    return true;
  }

  // Otherwise, fail.
  return false;
}

}  // namespace privacy_sandbox
