// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/browser/url_param_filter/url_param_filterer.h"

#include "base/base64.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

namespace {
std::map<std::string,
         std::map<FilterClassification::UseCase, std::vector<std::string>>>
ConvertToDefaultUseCases(
    const std::map<std::string, std::vector<std::string>>& source) {
  std::map<std::string,
           std::map<FilterClassification::UseCase, std::vector<std::string>>>
      source_with_use_cases;
  for (auto i : source) {
    source_with_use_cases[i.first][FilterClassification::USE_CASE_UNKNOWN] =
        i.second;
  }
  return source_with_use_cases;
}
}  // namespace

ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source,
    FilterClassification_SiteRole role) {
  url_param_filter::ClassificationMap result;
  for (auto i : source) {
    for (auto j : i.second) {
      url_param_filter::FilterClassification classification;
      classification.set_site(i.first);
      classification.set_site_role(role);
      if (j.first != FilterClassification::USE_CASE_UNKNOWN) {
        classification.add_use_cases(j.first);
      }
      for (auto k : j.second) {
        url_param_filter::FilterParameter* parameter =
            classification.add_parameters();
        parameter->set_name(k);
      }
      result[i.first][j.first] = classification;
    }
  }
  return result;
}

ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string, std::vector<std::string>>& source,
    url_param_filter::FilterClassification_SiteRole role) {
  return CreateClassificationMapForTesting(ConvertToDefaultUseCases(source),
                                           role);
}

std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_params,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& destination_params) {
  url_param_filter::FilterClassifications classifications;
  for (auto i : CreateClassificationMapForTesting(
           source_params, url_param_filter::FilterClassification_SiteRole::
                              FilterClassification_SiteRole_SOURCE)) {
    for (auto j : i.second) {
      *classifications.add_classifications() = std::move(j.second);
    }
  }
  for (auto i : CreateClassificationMapForTesting(
           destination_params, url_param_filter::FilterClassification_SiteRole::
                                   FilterClassification_SiteRole_DESTINATION)) {
    for (auto j : i.second) {
      *classifications.add_classifications() = std::move(j.second);
    }
  }
  return classifications.SerializeAsString();
}

std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params) {
  return CreateSerializedUrlParamFilterClassificationForTesting(
      ConvertToDefaultUseCases(source_params),
      ConvertToDefaultUseCases(destination_params));
}

std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_params,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& destination_params) {
  std::string compressed;
  compression::GzipCompress(
      CreateSerializedUrlParamFilterClassificationForTesting(
          source_params, destination_params),
      &compressed);
  std::string out;
  base::Base64Encode(compressed, &out);
  return out;
}

std::string CreateBase64EncodedFilterParamClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params) {
  std::string compressed;
  compression::GzipCompress(
      CreateSerializedUrlParamFilterClassificationForTesting(
          source_params, destination_params),
      &compressed);
  std::string out;
  base::Base64Encode(compressed, &out);
  return out;
}

FilterClassifications MakeClassificationsProtoFromMapWithUseCases(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_map,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& dest_map) {
  url_param_filter::FilterClassifications classifications;
  for (const auto& [site, params] : source_map) {
    for (const auto& [use_case, params] : params) {
      AddClassification(classifications.add_classifications(), site,
                        FilterClassification_SiteRole_SOURCE, params,
                        {use_case});
    }
  }
  for (const auto& [site, params] : dest_map) {
    for (const auto& [use_case, params] : params) {
      AddClassification(classifications.add_classifications(), site,
                        FilterClassification_SiteRole_DESTINATION, params,
                        {use_case});
    }
  }
  return classifications;
}

FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<std::string, std::vector<std::string>>& source_map,
    const std::map<std::string, std::vector<std::string>>& dest_map) {
  url_param_filter::FilterClassifications classifications;
  std::vector<FilterClassification::UseCase> use_cases;
  for (const auto& [site, params] : source_map) {
    AddClassification(classifications.add_classifications(), site,
                      FilterClassification_SiteRole_SOURCE, params, use_cases);
  }
  for (const auto& [site, params] : dest_map) {
    AddClassification(classifications.add_classifications(), site,
                      FilterClassification_SiteRole_DESTINATION, params,
                      use_cases);
  }
  return classifications;
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    const std::vector<std::string>& params) {
  std::vector<FilterClassification::UseCase> use_cases;
  FilterClassification fc;
  AddClassification(&fc, site, role, params, use_cases);
  return fc;
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases) {
  FilterClassification fc;
  AddClassification(&fc, site, role, params, use_cases);
  return fc;
}

void AddClassification(
    FilterClassification* classification,
    const std::string& site,
    FilterClassification_SiteRole role,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases) {
  classification->set_site(site);
  classification->set_site_role(role);
  for (const FilterClassification::UseCase& use_case : use_cases) {
    classification->add_use_cases(use_case);
  }
  for (const std::string& param : params) {
    FilterParameter* parameters = classification->add_parameters();
    parameters->set_name(param);
  }
}

}  // namespace url_param_filter
