// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser.h"

#include <string>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {
void InsertAPI(
    privacy_sandbox::PrivacySandboxAttestationsGatedAPISet& allowed_api_set,
    privacy_sandbox::PrivacySandboxAttestationsGatedAPIProto proto_api) {
  // If the proto enum matches one understood by the client, add it to the
  // allowed API set. Otherwise, ignore it.
  switch (proto_api) {
    case privacy_sandbox::TOPICS: {
      allowed_api_set.Put(
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics);
      return;
    }
    case privacy_sandbox::PROTECTED_AUDIENCE: {
      allowed_api_set.Put(privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                              kProtectedAudience);
      return;
    }
    case privacy_sandbox::PRIVATE_AGGREGATION: {
      allowed_api_set.Put(privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                              kPrivateAggregation);
      return;
    }
    case privacy_sandbox::ATTRIBUTION_REPORTING: {
      allowed_api_set.Put(privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                              kAttributionReporting);
      return;
    }
    case privacy_sandbox::SHARED_STORAGE: {
      allowed_api_set.Put(
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kSharedStorage);
      return;
    }
    case privacy_sandbox::LOCAL_UNPARTITIONED_DATA_ACCESS: {
      if (base::FeatureList::IsEnabled(
              blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
        allowed_api_set.Put(
            privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                kLocalUnpartitionedDataAccess);
      }
      return;
    }
    case privacy_sandbox::UNKNOWN: {
      return;
    }
    default: {
      return;
    }
  }
}
}  // namespace

namespace privacy_sandbox {

std::optional<PrivacySandboxAttestationsMap> ParseAttestationsFromString(
    std::string& input) {
  PrivacySandboxAttestationsProto proto;

  // Parse the istream into a proto for the attestations message format.
  if (!proto.ParseFromString(input)) {
    // Parsing failed. This should never happen in real use, because the input
    // comes from Chrome servers.
    return std::nullopt;
  }

  // Convert the parsed proto into a C++ attestations map.

  // Parse the set of "all APIs".
  PrivacySandboxAttestationsGatedAPISet all_api_set;
  for (int i = 0; i < proto.all_apis_size(); ++i) {
    InsertAPI(all_api_set, proto.all_apis(i));
  }

  // Allocate a vector to store the site attestation pairs from the proto.
  std::vector<
      std::pair<net::SchemefulSite, PrivacySandboxAttestationsGatedAPISet>>
      site_attestations_vector;
  site_attestations_vector.reserve(proto.sites_attested_for_all_apis_size() +
                                   proto.site_attestations_size());

  // Store the sites that are attested for all APIs.
  for (int i = 0; i < proto.sites_attested_for_all_apis_size(); ++i) {
    site_attestations_vector.emplace_back(
        net::SchemefulSite(GURL(proto.sites_attested_for_all_apis(i))),
        all_api_set);
  }

  // Store the sites that are attested for only some APIs.
  const auto& site_attestations_map = proto.site_attestations();
  for (const auto& site_attestation_pair : site_attestations_map) {
    // Get the site key.
    auto site = net::SchemefulSite(GURL(site_attestation_pair.first));

    // Collect all of the allowed apis for that site.
    const auto& attestation = site_attestation_pair.second;
    PrivacySandboxAttestationsGatedAPISet allowed_api_set;
    for (int i = 0; i < attestation.attested_apis_size(); ++i) {
      InsertAPI(allowed_api_set, attestation.attested_apis(i));
    }

    // Append the site,apis pair to the C++ formatted vector.
    site_attestations_vector.emplace_back(site, allowed_api_set);
  }

  // Convert the vector into a flat map (which prefers initialization with the
  // entire data structure, not incremental inserts) and return.
  return PrivacySandboxAttestationsMap(site_attestations_vector);
}

}  // namespace privacy_sandbox
