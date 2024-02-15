// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

bool ParseLookupAffiliationResponse(
    const std::vector<FacetURI>& requested_facet_uris,
    const affiliation_pb::LookupAffiliationByHashPrefixResponse& response,
    AffiliationFetcherDelegate::Result* result);

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_
