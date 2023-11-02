// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTER_TEST_HELPER_H_
#define COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTER_TEST_HELPER_H_

#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filterer.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace url_param_filter {

MATCHER_P(EqualsProto,
          want,
          "Matches an argument against an expected a proto Message.") {
  return arg.SerializeAsString() == want.SerializeAsString();
}

// A helper to easily create URL param filter classification maps based
// on the passed-in source. `source` should map an eTLD+1 or wildcard to a
// vector of params for the given role. For example, for eTLD+1 source.xyz, when
// observed as the source (aka referer) of a navigation, block params
// "plzblock" and "plzblock1".
url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source,
    url_param_filter::FilterClassification_SiteRole role);

// Equivalent to the other overload, but uses the default (unknown) use case for
// all parameters.
url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string, std::vector<std::string>>& source,
    url_param_filter::FilterClassification_SiteRole role);

// A helper to easily create URL param filter classification maps based
// on the passed-in source. `source` should map a ClassificationMapKey to a
// vector of params. Gives all parameters the default (unknown) use case and the
// NON_EXPERIMENTAL tag.
url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<ClassificationMapKey, std::vector<std::string>>& source);

// Creates and serializes the URL param filter classifications proto.
// Used for simulating reading the classifications file from Component Updater.
std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_params,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& destination_params,
    const std::vector<std::string>& experiment_tags);

// Equivalent to the other overload, but uses empty use case lists for all
// parameters.
std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params,
    const std::vector<std::string>& experiment_tags);

// Create a base64 representation of the URL param filter classifications
// proto. Used for initialization of the feature params in tests.
std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_params,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& destination_params);

// Equivalent to the other overload, but uses empty use case lists for all
// parameters.
std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params);

// Make a FilterClassifications proto using two maps, for source and destination
// classifications. Each map takes the form "site"->["p1", "p2", ...] where
// each "pi" in the list is a param that should be filtered from that site.
// Defaults match type to EXACT_ETLD_PLUS_ONE for each created classification.
FilterClassifications MakeClassificationsProtoFromMapWithUseCases(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_map,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& dest_map);

// Make a FilterClassifications proto using a map of all classifications.
// Each map takes the form ClassificationMapKey->["p1", "p2", ...] where
// each "pi" in the list is a param that should be filtered from that site.
FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<ClassificationMapKey, std::vector<std::string>>& map);

// Equivalent to the other overload, but uses empty use case lists for all
// parameters.  Defaults site_match_type to EXACT_ETLD_PLUS_ONE.
FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<std::string, std::vector<std::string>>& source_map,
    const std::map<std::string, std::vector<std::string>>& dest_map);

// Make a FilterClassification proto provided a site, role, match type, and list
// of params.
FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases);

// Equivalent to the other overload, but uses an empty list of use cases.
FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params);

// Make a FilterClassification proto provided a site, role, match type,
// experiment override, and list of params.
FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases,
    const std::string& experiment_identifier);

// Helper method for adding repeated classifications on a FilterClassification.
void AddClassification(
    FilterClassification* classification,
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases,
    const std::vector<std::string>& experiment_tags);
}  // namespace url_param_filter
#endif  // COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTER_TEST_HELPER_H_
