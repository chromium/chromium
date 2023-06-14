// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"

namespace privacy_sandbox {

// When a new enum value is added:
// 1. Update kMaxValue to match it.
// 2. Update `PrivacySandboxAttestationsGatedAPIProto` in
//    `privacy_sandbox_attestations.proto`.
// 3. Update `AllowAPI` in `privacy_sandbox_attestations_parser.cc`.
enum class PrivacySandboxAttestationsGatedAPI {
  kTopics,
  kProtectedAudience,
  kPrivateAggregation,
  kAttributionReporting,
  kSharedStorage,

  kMaxValue = kSharedStorage,
};

using PrivacySandboxAttestationsGatedAPISet =
    base::EnumSet<PrivacySandboxAttestationsGatedAPI,
                  PrivacySandboxAttestationsGatedAPI::kTopics,
                  PrivacySandboxAttestationsGatedAPI::kMaxValue>;

// TODO(crbug.com/1454847): Add a concise representation for "this site is
// attested for all APIs".
using PrivacySandboxAttestationsMap =
    base::flat_map<net::SchemefulSite, PrivacySandboxAttestationsGatedAPISet>;

class PrivacySandboxAttestations {
 public:
  explicit PrivacySandboxAttestations(
      const PrivacySandboxAttestationsMap& attestations_map);
  ~PrivacySandboxAttestations();
  PrivacySandboxAttestations(PrivacySandboxAttestations&) = delete;

  // Returns whether `site` is enrolled and attested for `invoking_api`.
  // (If the `kEnforcePrivacySandboxAttestations` flag is disabled, returns
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
