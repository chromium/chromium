// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_TEST_HELPER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_TEST_HELPER_H_

#include "chrome/browser/url_param_filter/url_param_filterer.h"

namespace url_param_filter {
// A helper to easily create URL param filter classification maps based
// on the passed-in source. `source` should map an eTLD+1 to a vector
// of params for the given role. For example, for eTLD+1 source.xyz, when
// observed as the source (aka referer) of a navigation, block params
// "plzblock" and "plzblock1".
url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string, std::vector<std::string>>& source,
    url_param_filter::FilterClassification_SiteRole role);

// Create a base64 representation of the URL param filter classifications
// proto. Used for initialization of the feature params in tests.
std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params);

// Make a FilterClassifications proto using two maps, for source and destination
// classifications. Each map takes the form "site"->["p1", "p2", ...] where
// each "pi" in the list is a param that should be filtered from that site.
FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<std::string, std::vector<std::string>>& source_map,
    const std::map<std::string, std::vector<std::string>>& dest_map);

// Make a FilterClassification proto provided a site, role, and list of params.
FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    const std::vector<std::string>& params);

// Helper method for adding repeated classifications on a FilterClassification.
void AddClassification(FilterClassification* classification,
                       const std::string& site,
                       FilterClassification_SiteRole role,
                       const std::vector<std::string>& params);
}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_TEST_HELPER_H_
