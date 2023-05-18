// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"

namespace privacy_sandbox {

enum class PrivacySandboxAttestationsGatedAPI {
  kTopics,
  kProtectedAudience,
  kPrivateAggregation,
  kAttributionReporting,
  kSharedStorage,

  // Update this value whenever a new API is added.
  kMaxValue = kSharedStorage,
};

using PrivacySandboxAttestationsMap = base::flat_map<
    net::SchemefulSite,
    base::EnumSet<PrivacySandboxAttestationsGatedAPI,
                  PrivacySandboxAttestationsGatedAPI::kTopics,
                  PrivacySandboxAttestationsGatedAPI::kMaxValue>>;

class PrivacySandboxAttestations {
 public:
  explicit PrivacySandboxAttestations(
      const PrivacySandboxAttestationsMap& attestations_map);
  ~PrivacySandboxAttestations();
  PrivacySandboxAttestations(PrivacySandboxAttestations&) = delete;

  // Returns whether `site` is enrolled and attested for `invoking_api`.
  // (If the `kEnforcePrivacySandboxAttestations` flag is enabled, returns
  // true unconditionally.)
  bool IsSiteAttested(net::SchemefulSite site,
                      PrivacySandboxAttestationsGatedAPI invoking_api) const;

  void AddOverride(net::SchemefulSite site) { overrides_.push_back(site); }
  const std::vector<net::SchemefulSite> GetOverridesForTesting() const {
    return overrides_;
  }

 private:
  PrivacySandboxAttestationsMap attestations_map_;
  std::vector<net::SchemefulSite> overrides_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H
