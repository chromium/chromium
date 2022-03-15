// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filterer.h"

#include <vector>

#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "chrome/common/chrome_features.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace url_param_filter {
namespace {

// Get the ETLD+1 of the URL, which means any subdomain is treated equivalently.
std::string GetEtldPlusOne(const GURL& gurl) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      gurl, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Add the params classified as requiring filtering into the given parameter
// set.
void AddParams(std::set<std::string>& parameter_set,
               url_param_filter::FilterClassification classification) {
  for (auto i : classification.parameters()) {
    parameter_set.insert(i.name());
  }
}

// Filter a given URL according to the passed-in classifications, optionally
// checking any encoded, nested URLs.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& source_classification_map,
                       const ClassificationMap& destination_classification_map,
                       bool check_nested) {
  GURL result = GURL{destination_url};
  int filtered_params_count = 0;

  std::string source_etld_plus1 = GetEtldPlusOne(source_url);
  std::string destination_etld_plus1 = GetEtldPlusOne(destination_url);

  std::set<std::string> blocked_parameters;
  // Check whether source eTLD+1 has params classified as requiring filtering.
  // If so, and the params are present on the destination URL, or any nested
  // URLs, remove them.
  auto source_classification_result =
      source_classification_map.find(source_etld_plus1);
  if (source_classification_result != source_classification_map.end()) {
    AddParams(blocked_parameters, source_classification_result->second);
  }
  auto destination_classification_result =
      destination_classification_map.find(destination_etld_plus1);
  if (destination_classification_result !=
      destination_classification_map.end()) {
    AddParams(blocked_parameters, destination_classification_result->second);
  }
  // Return quickly if there are no parameters we care about.
  if (blocked_parameters.size() == 0) {
    return FilterResult{destination_url, filtered_params_count};
  }

  std::vector<std::string> query_parts;
  for (net::QueryIterator it(result); !it.IsAtEnd(); it.Advance()) {
    const std::string key = std::string{it.GetKey()};
    // If we don't find the given param in our set of blocked parameters, we can
    // add it to the result safely.
    if (blocked_parameters.find(base::ToLowerASCII(key)) ==
        blocked_parameters.end()) {
      std::string value = std::string{it.GetValue()};
      if (check_nested) {
        GURL nested = GURL{net::UnescapeBinaryURLComponent(value)};
        if (nested.is_valid()) {
          FilterResult nested_result =
              FilterUrl(destination_url, nested, source_classification_map,
                        destination_classification_map, false);
          // If a nested URL contains a param we must filter, do so now.
          if (nested != nested_result.filtered_url) {
            value = net::EscapeQueryParamValue(
                nested_result.filtered_url.spec(), /*use_plus=*/false);
            filtered_params_count += nested_result.filtered_param_count;
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
    }
  }

  std::string new_query = base::JoinString(query_parts, "&");
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  result = result.ReplaceComponents(replacements);
  return FilterResult{result, filtered_params_count};
}

// Write metrics about results of param filtering.
void WriteMetrics(FilterResult result) {
  base::UmaHistogramCounts100(
      "Navigation.UrlParamFilter.FilteredParamCountExperimental",
      result.filtered_param_count);
}
}  // anonymous namespace

FilterResult FilterUrl(
    const GURL& source_url,
    const GURL& destination_url,
    const ClassificationMap& source_classification_map,
    const ClassificationMap& destination_classification_map) {
  return FilterUrl(source_url, destination_url, source_classification_map,
                   destination_classification_map, true);
}

GURL FilterUrl(const GURL& source_url, const GURL& destination_url) {
  if (base::FeatureList::IsEnabled(features::kIncognitoParamFilterEnabled)) {
    FilterResult result = FilterUrl(
        source_url, destination_url,
        ClassificationsLoader::GetInstance()->GetSourceClassifications(),
        ClassificationsLoader::GetInstance()->GetDestinationClassifications());
    WriteMetrics(result);
    return result.filtered_url;
  }
  return destination_url;
}

}  // namespace url_param_filter
