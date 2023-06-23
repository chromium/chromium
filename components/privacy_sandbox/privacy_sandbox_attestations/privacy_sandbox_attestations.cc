// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace privacy_sandbox {

namespace {

// Global PrivacySandboxAttestations instance for testing.
PrivacySandboxAttestations* g_test_instance = nullptr;

// Helper function that checks if enrollment overrides are set from the
// chrome://flags entry.
bool IsOverriddenByFlags(const net::SchemefulSite& site) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(
          privacy_sandbox::kPrivacySandboxEnrollmentOverrides)) {
    return false;
  }

  std::string origins_str = command_line.GetSwitchValueASCII(
      privacy_sandbox::kPrivacySandboxEnrollmentOverrides);

  std::vector<std::string> overrides_list = base::SplitString(
      origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::string override_str : overrides_list) {
    if (override_str.empty()) {
      continue;
    }

    GURL override_url(override_str);
    if (!override_url.is_valid()) {
      continue;
    }

    if (net::SchemefulSite(override_url) == site) {
      return true;
    }
  }

  return false;
}

}  // namespace

// static
PrivacySandboxAttestations* PrivacySandboxAttestations::GetInstance() {
  if (g_test_instance) {
    return g_test_instance;
  }

  static base::NoDestructor<PrivacySandboxAttestations> instance;
  return instance.get();
}

// static
void PrivacySandboxAttestations::SetInstanceForTesting(
    PrivacySandboxAttestations* test_instance) {
  g_test_instance = test_instance;
}

// static
std::unique_ptr<PrivacySandboxAttestations>
PrivacySandboxAttestations::CreateForTesting() {
  std::unique_ptr<PrivacySandboxAttestations> test_instance(
      new PrivacySandboxAttestations());
  return test_instance;
}

PrivacySandboxAttestations::~PrivacySandboxAttestations() = default;

PrivacySandboxAttestations::PrivacySandboxAttestations(
    PrivacySandboxAttestations&&) = default;

PrivacySandboxAttestations& PrivacySandboxAttestations::operator=(
    PrivacySandboxAttestations&&) = default;

bool PrivacySandboxAttestations::IsSiteAttested(
    const net::SchemefulSite& site,
    PrivacySandboxAttestationsGatedAPI invoking_api) const {
  // If attestations aren't enabled, pass the check trivially.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    return true;
  }

  // Pass the check if the site is in the list of devtools overrides.
  if (IsOverridden(site)) {
    return true;
  }

  // When the attesations map is not present, the behavior is default-deny.
  if (!attestations_map_.has_value()) {
    return false;
  }

  // If `site` isn't enrolled at all, fail the check.
  auto it = attestations_map_->find(site);
  if (it == attestations_map_->end()) {
    return false;
  }

  // If `site` is attested for `invoking_api`, pass the check.
  if (it->second.Has(invoking_api)) {
    return true;
  }

  // Otherwise, fail.
  return false;
}

void PrivacySandboxAttestations::AddOverride(const net::SchemefulSite& site) {
  overridden_sites_.push_back(site);
}

bool PrivacySandboxAttestations::IsOverridden(
    const net::SchemefulSite& site) const {
  return IsOverriddenByFlags(site) || base::Contains(overridden_sites_, site);
}

void PrivacySandboxAttestations::SetAttestationsForTesting(
    PrivacySandboxAttestationsMap attestations_map) {
  attestations_map_ = std::move(attestations_map);
}

PrivacySandboxAttestations::PrivacySandboxAttestations() = default;

}  // namespace privacy_sandbox
