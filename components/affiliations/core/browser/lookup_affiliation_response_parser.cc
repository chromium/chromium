// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/lookup_affiliation_response_parser.h"

#include "base/containers/flat_set.h"

namespace affiliations {

namespace {

// Template for the affiliation_pb message:
// > affiliation_pb::Affiliation
// > affiliation_pb::FacetGroup
template <typename MessageT>
std::vector<Facet> ParseFacets(const MessageT& response) {
  std::vector<Facet> facets;
  facets.reserve(response.facet().size());
  for (const auto& facet : response.facet()) {
    const std::string& uri_spec(facet.id());
    FacetURI uri = FacetURI::FromPotentiallyInvalidSpec(uri_spec);
    // Ignore potential future kinds of facet URIs (e.g. for new platforms).
    if (!uri.is_valid())
      continue;
    Facet new_facet(uri);
    if (facet.has_branding_info()) {
      new_facet.branding_info = FacetBrandingInfo{
          facet.branding_info().name(), GURL(facet.branding_info().icon_url())};
    }
    if (facet.has_change_password_info()) {
      new_facet.change_password_url =
          GURL(facet.change_password_info().change_password_url());
    }
    if (facet.has_main_domain()) {
      new_facet.main_domain = facet.main_domain();
    }
    facets.push_back(std::move(new_facet));
  }
  return facets;
}

AffiliatedFacets ParseEqClass(const affiliation_pb::Affiliation& affiliation) {
  return ParseFacets(affiliation);
}

GroupedFacets ParseEqClass(const affiliation_pb::FacetGroup& grouping) {
  GroupedFacets group;
  group.facets = ParseFacets(grouping);
  if (grouping.has_group_branding_info()) {
    group.branding_info =
        FacetBrandingInfo{grouping.group_branding_info().name(),
                          GURL(grouping.group_branding_info().icon_url())};
  }
  return group;
}

void AddSingleFacet(std::vector<AffiliatedFacets>& affiliations, Facet facet) {
  affiliations.push_back({facet});
}

void AddSingleFacet(std::vector<GroupedFacets>& groups, Facet facet) {
  GroupedFacets group;
  group.facets = {facet};
  groups.push_back(std::move(group));
}

// Template for the affiliation_pb message:
// > affiliation_pb::Affiliation
// > affiliation_pb::FacetGroup
template <typename MessageT, typename ResultT>
bool ParseResponse(const std::vector<FacetURI>& requested_facet_uris,
                   const MessageT& response,
                   ResultT& result) {
  std::map<std::string, size_t> facet_uri_to_class_index;
  base::flat_set<std::string> requested_facets = base::MakeFlatSet<std::string>(
      requested_facet_uris, /*comp=*/{}, &FacetURI::potentially_invalid_spec);

  // Validate and de-duplicate data.
  for (const auto& equivalence_class : response) {
    // Be lenient and ignore empty (after filtering) equivalence classes.
    if (equivalence_class.facet().empty())
      continue;

    // Ignore equivalence classes that are duplicates of earlier ones. However,
    // fail in the case of a partial overlap, which violates the invariant that
    // affiliations must form an equivalence relation. Also check, if the class
    // was requested.
    bool is_class_requested = false;
    for (const auto& facet : equivalence_class.facet()) {
      if (requested_facets.count(facet.id()))
        is_class_requested = true;

      if (!facet_uri_to_class_index.count(facet.id()))
        facet_uri_to_class_index[facet.id()] = result.size();

      if (facet_uri_to_class_index[facet.id()] !=
          facet_uri_to_class_index[equivalence_class.facet()[0].id()]) {
        return false;
      }
    }

    // Filter out duplicate or unrequested equivalence classes in the response.
    if (is_class_requested &&
        facet_uri_to_class_index[equivalence_class.facet()[0].id()] ==
            result.size()) {
      result.push_back(ParseEqClass(equivalence_class));
    }
  }

  // Synthesize an equivalence class (of size one) for each facet that did not
  // appear in the server response due to not being affiliated with any others.
  for (const FacetURI& uri : requested_facet_uris) {
    if (!facet_uri_to_class_index.count(uri.potentially_invalid_spec()))
      AddSingleFacet(result, Facet(uri));
  }

  return true;
}

}  // namespace

bool ParseLookupAffiliationResponse(
    const std::vector<FacetURI>& requested_facet_uris,
    const affiliation_pb::LookupAffiliationByHashPrefixResponse& response,
    AffiliationFetcherDelegate::Result* result) {
  for (const auto& domain : response.psl_extensions()) {
    result->psl_extensions.push_back(domain);
  }
  return ParseResponse(requested_facet_uris, response.affiliations(),
                       result->affiliations) &&
         ParseResponse(requested_facet_uris, response.groups(),
                       result->groupings);
}

}  // namespace affiliations
