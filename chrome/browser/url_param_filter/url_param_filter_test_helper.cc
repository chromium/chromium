// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/browser/url_param_filter/url_param_filterer.h"

#include "base/base64.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string, std::vector<std::string>>& source,
    url_param_filter::FilterClassification_SiteRole role) {
  url_param_filter::ClassificationMap result;
  for (auto i : source) {
    url_param_filter::FilterClassification classification;
    classification.set_site(i.first);
    classification.set_site_role(role);
    for (auto j : i.second) {
      url_param_filter::FilterParameter* parameter =
          classification.add_parameters();
      parameter->set_name(j);
    }
    result[i.first] = classification;
  }
  return result;
}

std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params) {
  url_param_filter::FilterClassifications classifications;
  for (auto i : CreateClassificationMapForTesting(
           source_params, url_param_filter::FilterClassification_SiteRole::
                              FilterClassification_SiteRole_SOURCE)) {
    *classifications.add_classifications() = std::move(i.second);
  }
  for (auto i : CreateClassificationMapForTesting(
           destination_params, url_param_filter::FilterClassification_SiteRole::
                                   FilterClassification_SiteRole_DESTINATION)) {
    *classifications.add_classifications() = std::move(i.second);
  }
  std::string compressed;
  compression::GzipCompress(classifications.SerializeAsString(), &compressed);
  std::string out;
  base::Base64Encode(compressed, &out);
  return out;
}

FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<std::string, std::vector<std::string>>& source_map,
    const std::map<std::string, std::vector<std::string>>& dest_map) {
  url_param_filter::FilterClassifications classifications;
  for (const auto& [site, params] : source_map) {
    AddClassification(classifications.add_classifications(), site,
                      FilterClassification_SiteRole_SOURCE, params);
  }
  for (const auto& [site, params] : dest_map) {
    AddClassification(classifications.add_classifications(), site,
                      FilterClassification_SiteRole_DESTINATION, params);
  }
  return classifications;
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    const std::vector<std::string>& params) {
  FilterClassification fc;
  AddClassification(&fc, site, role, params);
  return fc;
}

void AddClassification(FilterClassification* classification,
                       const std::string& site,
                       FilterClassification_SiteRole role,
                       const std::vector<std::string>& params) {
  classification->set_site(site);
  classification->set_site_role(role);
  for (const std::string& param : params) {
    FilterParameter* parameters = classification->add_parameters();
    parameters->set_name(param);
  }
}

}  // namespace url_param_filter
