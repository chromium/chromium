// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/hash_affiliation_fetcher.h"

#include <memory>

#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {
const int kPrefixLength = 16;

uint64_t ComputeHashPrefix(const password_manager::FacetURI& uri) {
  static_assert(kPrefixLength < 64,
                "Prefix should not be longer than 8 bytes.");

  int bytes_count = kPrefixLength / 8 + (kPrefixLength % 8 != 0);

  uint8_t hash[bytes_count];
  crypto::SHA256HashString(uri.canonical_spec(), hash, bytes_count);
  uint64_t result = 0;

  for (int i = 0; i < bytes_count; i++) {
    result <<= 8;
    result |= hash[i];
  }

  int bits_to_clear = kPrefixLength % 8;
  result &= (~0 << bits_to_clear);

  int bits_to_shift = ((8 - bytes_count) * 8);
  result <<= bits_to_shift;

  return result;
}

}  // namespace

namespace password_manager {

HashAffiliationFetcher::HashAffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : AffiliationFetcherBase(std::move(url_loader_factory), delegate) {}

HashAffiliationFetcher::~HashAffiliationFetcher() = default;

void HashAffiliationFetcher::StartRequest(
    const std::vector<FacetURI>& facet_uris,
    RequestInfo request_info) {
  requested_facet_uris_ = facet_uris;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("affiliation_lookup_by_hash", R"(
        semantics {
          sender: "Hash Affiliation Fetcher"
          description:
            " Chrome can obtain information about affiliated and grouped "
            " websites as well as link to directly change password using this "
            " request. Chrome sends only hash prefixes of the websites. "
          trigger: "Whenever a new password added or one day passed after last"
            " request for existing passwords. Another trigger is a change "
            " password action in settings."
          data:
            "Hash prefixes of websites URLs or package name for android apps."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is used to facilitate password manager filling "
            "experience by allowing users to fill passwords between "
            "affiliated sites and apps, or when user needs to get a direct"
            "change password URL. Furthermore only deleting all passwords will "
            "turn this feature off."
          policy_exception_justification:
            "Not implemented. Sending only hash prefixes to the server allows "
            "to preserve users' privacy. "
        })");

  // Prepare the payload based on |facet_uris| and |request_info|.
  affiliation_pb::LookupAffiliationByHashPrefixRequest lookup_request;

  lookup_request.set_hash_prefix_length(kPrefixLength);

  for (const FacetURI& uri : facet_uris)
    lookup_request.add_hash_prefixes(ComputeHashPrefix(uri));

  *lookup_request.mutable_mask() = CreateLookupMask(request_info);

  std::string serialized_request;
  bool success = lookup_request.SerializeToString(&serialized_request);
  DCHECK(success);

  FinalizeRequest(serialized_request, BuildQueryURL(), traffic_annotation);
}

const std::vector<FacetURI>& HashAffiliationFetcher::GetRequestedFacetURIs()
    const {
  return requested_facet_uris_;
}

// static
GURL HashAffiliationFetcher::BuildQueryURL() {
  return net::AppendQueryParameter(
      GURL("https://www.googleapis.com/affiliation/v1/"
           "affiliation:lookupByHashPrefix"),
      "key", google_apis::GetAPIKey());
}

}  // namespace password_manager
