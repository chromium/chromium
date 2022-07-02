// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/url_param_filterer.h"

#include <vector>

#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace url_param_filter {
namespace {

// Get the ETLD+1 of the URL, which means any subdomain is treated equivalently.
// IP addresses are returned verbatim. Note that this is schemeless, so
// filtering is applied equivalently regardless of http vs https vs others.
std::string GetClassifiedSite(const GURL& gurl) {
  if (gurl.HostIsIPAddress()) {
    return gurl.host();
  }
  return net::registry_controlled_domains::GetDomainAndRegistry(
      gurl, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Filter a given URL according to the passed-in classifications, optionally
// checking any encoded, nested URLs.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& source_classification_map,
                       const ClassificationMap& destination_classification_map,
                       const bool check_nested,
                       const FilterClassification::UseCase use_case) {
  GURL result = GURL{destination_url};
  int filtered_params_count = 0;
  ClassificationExperimentStatus experiment_status =
      ClassificationExperimentStatus::NON_EXPERIMENTAL;

  // If there's no query string, we can short-circuit immediately.
  if (!destination_url.has_query()) {
    return FilterResult{destination_url, filtered_params_count,
                        experiment_status};
  }

  std::string source_classified_site = GetClassifiedSite(source_url);
  std::string destination_classified_site = GetClassifiedSite(destination_url);

  std::map<std::string, ClassificationExperimentStatus> blocked_parameters;
  // Check whether source site, as seen by the classifier (eTLD+1 or IP), has
  // params classified as requiring filtering. If so, and the params are present
  // on the destination URL, or any nested URLs, remove them.
  auto source_classification_result =
      source_classification_map.find(source_classified_site);
  if (source_classification_result != source_classification_map.end()) {
    auto source_classification_with_use_case =
        source_classification_result->second.find(use_case);
    if (source_classification_with_use_case !=
        source_classification_result->second.end()) {
      blocked_parameters.insert(
          source_classification_with_use_case->second.begin(),
          source_classification_with_use_case->second.end());
    }
  }
  auto destination_classification_result =
      destination_classification_map.find(destination_classified_site);
  if (destination_classification_result !=
      destination_classification_map.end()) {
    auto destination_classification_with_use_case =
        destination_classification_result->second.find(use_case);
    if (destination_classification_with_use_case !=
        destination_classification_result->second.end()) {
      blocked_parameters.insert(
          destination_classification_with_use_case->second.begin(),
          destination_classification_with_use_case->second.end());
    }
  }
  // Return quickly if there are no parameters we care about.
  if (blocked_parameters.size() == 0) {
    return FilterResult{destination_url, filtered_params_count,
                        experiment_status};
  }

  std::vector<std::string> query_parts;
  for (net::QueryIterator it(result); !it.IsAtEnd(); it.Advance()) {
    const std::string key = std::string{it.GetKey()};
    // If we don't find the given param in our set of blocked parameters, we can
    // add it to the result safely.
    auto classification = blocked_parameters.find(base::ToLowerASCII(key));
    if (classification == blocked_parameters.end()) {
      std::string value = std::string{it.GetValue()};
      if (check_nested) {
        GURL nested = GURL{base::UnescapeBinaryURLComponent(value)};
        if (nested.is_valid()) {
          FilterResult nested_result =
              FilterUrl(destination_url, nested, source_classification_map,
                        destination_classification_map, false, use_case);
          // If a nested URL contains a param we must filter, do so now.
          if (nested != nested_result.filtered_url) {
            value = base::EscapeQueryParamValue(
                nested_result.filtered_url.spec(), /*use_plus=*/false);
            filtered_params_count += nested_result.filtered_param_count;
            if (nested_result.experimental_status ==
                ClassificationExperimentStatus::EXPERIMENTAL) {
              experiment_status = ClassificationExperimentStatus::EXPERIMENTAL;
            }
          }
        }
      }
      if (value != "") {
        query_parts.push_back(base::StrCat({key, "=", value}));
      } else {
        query_parts.push_back(key);
      }
    } else {
      filtered_params_count++;
      if (classification->second ==
          ClassificationExperimentStatus::EXPERIMENTAL) {
        experiment_status = classification->second;
      }
    }
  }

  std::string new_query = base::JoinString(query_parts, "&");
  GURL::Replacements replacements;
  if (new_query == "") {
    replacements.ClearQuery();
  } else {
    replacements.SetQueryStr(new_query);
  }
  result = result.ReplaceComponents(replacements);
  return FilterResult{result, filtered_params_count, experiment_status};
}
}  // anonymous namespace

FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& source_classification_map,
                       const ClassificationMap& destination_classification_map,
                       const FilterClassification::UseCase use_case) {
  return FilterUrl(source_url, destination_url, source_classification_map,
                   destination_classification_map, true, use_case);
}

FilterResult FilterUrl(const GURL& source_url, const GURL& destination_url) {
  if (!base::FeatureList::IsEnabled(features::kIncognitoParamFilterEnabled)) {
    return FilterResult{destination_url, 0};
  }
  return FilterUrl(
      source_url, destination_url,
      ClassificationsLoader::GetInstance()->GetSourceClassifications(),
      ClassificationsLoader::GetInstance()->GetDestinationClassifications(),
      FilterClassification::USE_CASE_UNKNOWN);
}

FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const FilterClassification::UseCase use_case) {
  if (!base::FeatureList::IsEnabled(features::kIncognitoParamFilterEnabled)) {
    return FilterResult{destination_url, 0,
                        ClassificationExperimentStatus::NON_EXPERIMENTAL};
  }
  return FilterUrl(
      source_url, destination_url,
      ClassificationsLoader::GetInstance()->GetSourceClassifications(),
      ClassificationsLoader::GetInstance()->GetDestinationClassifications(),
      use_case);
}

}  // namespace url_param_filter
