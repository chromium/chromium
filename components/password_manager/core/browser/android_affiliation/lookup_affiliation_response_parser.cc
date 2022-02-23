// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/lookup_affiliation_response_parser.h"

namespace password_manager {

namespace {

// Template for the affiliation_pb message:
// > affiliation_pb::Affiliation
// > affiliation_pb::FacetGroup
template <typename MessageT>
bool ParseFacets(const std::vector<FacetURI>& requested_facet_uris,
                 const MessageT& response,
                 std::vector<std::vector<Facet>>& result) {
  std::map<FacetURI, size_t> facet_uri_to_class_index;
  for (const auto& equivalence_class : response) {
    std::vector<Facet> facets;
    facets.reserve(equivalence_class.facet().size());
    for (const auto& facet : equivalence_class.facet()) {
      const std::string& uri_spec(facet.id());
      FacetURI uri = FacetURI::FromPotentiallyInvalidSpec(uri_spec);
      // Ignore potential future kinds of facet URIs (e.g. for new platforms).
      if (!uri.is_valid())
        continue;
      Facet new_facet = {uri};
      if (facet.has_branding_info()) {
        new_facet.branding_info =
            FacetBrandingInfo{facet.branding_info().name(),
                              GURL(facet.branding_info().icon_url())};
      }
      if (facet.has_change_password_info()) {
        new_facet.change_password_url =
            GURL(facet.change_password_info().change_password_url());
      }
      facets.push_back(std::move(new_facet));
    }

    // Be lenient and ignore empty (after filtering) equivalence classes.
    if (facets.empty())
      continue;

    // Ignore equivalence classes that are duplicates of earlier ones. However,
    // fail in the case of a partial overlap, which violates the invariant that
    // affiliations must form an equivalence relation.
    for (const Facet& facet : facets) {
      if (!facet_uri_to_class_index.count(facet.uri))
        facet_uri_to_class_index[facet.uri] = result.size();
      if (facet_uri_to_class_index[facet.uri] !=
          facet_uri_to_class_index[facets[0].uri]) {
        return false;
      }
    }

    // Filter out duplicate equivalence classes in the response.
    if (facet_uri_to_class_index[facets[0].uri] == result.size()) {
      result.push_back(std::move(facets));
    }
  }

  // Synthesize an equivalence class (of size one) for each facet that did not
  // appear in the server response due to not being affiliated with any others.
  for (const FacetURI& uri : requested_facet_uris) {
    if (!facet_uri_to_class_index.count(uri))
      result.push_back({{uri}});
  }

  return true;
}

}  // namespace

bool ParseLookupAffiliationResponse(
    const std::vector<FacetURI>& requested_facet_uris,
    const affiliation_pb::LookupAffiliationByHashPrefixResponse& response,
    AffiliationFetcherDelegate::Result* result) {
  return ParseFacets(requested_facet_uris, response.affiliations(),
                     result->affiliations) &&
         ParseFacets(requested_facet_uris, response.groups(),
                     result->groupings);
}

}  // namespace password_manager
