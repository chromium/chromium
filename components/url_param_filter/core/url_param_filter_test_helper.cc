// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/url_param_filter_test_helper.h"

#include "base/base64.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filterer.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

namespace {
constexpr char DEFAULT_TAG[] = "default";
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

// Create a ClassificationMap with the default non-experimental status.
ClassificationMap CreateClassificationMapForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source,
    FilterClassification_SiteRole role) {
  url_param_filter::ClassificationMap result;
  for (auto i : source) {
    for (auto j : i.second) {
      for (auto k : j.second) {
        result[{.site_role = role, .site = i.first}][j.first][k] =
            ClassificationExperimentStatus::NON_EXPERIMENTAL;
      }
    }
  }
  return result;
}

url_param_filter::ClassificationMap CreateClassificationMapForTesting(
    const std::map<ClassificationMapKey, std::vector<std::string>>& source) {
  url_param_filter::ClassificationMap result;
  for (const auto& [key, params] : source) {
    for (const auto& param : params) {
      result[key][FilterClassification::USE_CASE_UNKNOWN][param] =
          ClassificationExperimentStatus::NON_EXPERIMENTAL;
    }
  }
  return result;
}

std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& source_params,
    const std::map<std::string,
                   std::map<FilterClassification::UseCase,
                            std::vector<std::string>>>& destination_params,
    const std::vector<std::string>& experiment_tags) {
  url_param_filter::FilterClassifications classifications;
  for (auto [key, inner_map] : CreateClassificationMapForTesting(
           source_params, url_param_filter::FilterClassification_SiteRole::
                              FilterClassification_SiteRole_SOURCE)) {
    for (auto j : inner_map) {
      url_param_filter::FilterClassification classification;
      classification.set_site(key.site);
      for (auto i : experiment_tags) {
        classification.add_experiment_tags(i);
      }
      classification.set_site_role(
          url_param_filter::FilterClassification::SOURCE);
      if (j.first != FilterClassification::USE_CASE_UNKNOWN) {
        classification.add_use_cases(j.first);
      }
      for (auto k : j.second) {
        url_param_filter::FilterParameter* parameter =
            classification.add_parameters();
        parameter->set_name(k.first);
      }
      *classifications.add_classifications() = std::move(classification);
    }
  }
  for (auto [key, inner_map] : CreateClassificationMapForTesting(
           destination_params, url_param_filter::FilterClassification_SiteRole::
                                   FilterClassification_SiteRole_DESTINATION)) {
    for (auto j : inner_map) {
      url_param_filter::FilterClassification classification;
      classification.set_site(key.site);
      for (auto i : experiment_tags) {
        classification.add_experiment_tags(i);
      }
      classification.set_site_role(
          url_param_filter::FilterClassification::DESTINATION);
      if (j.first != FilterClassification::USE_CASE_UNKNOWN) {
        classification.add_use_cases(j.first);
      }
      for (auto k : j.second) {
        url_param_filter::FilterParameter* parameter =
            classification.add_parameters();
        parameter->set_name(k.first);
      }
      *classifications.add_classifications() = std::move(classification);
    }
  }
  return classifications.SerializeAsString();
}

std::string CreateSerializedUrlParamFilterClassificationForTesting(
    const std::map<std::string, std::vector<std::string>>& source_params,
    const std::map<std::string, std::vector<std::string>>& destination_params,
    const std::vector<std::string>& experiment_tags) {
  return CreateSerializedUrlParamFilterClassificationForTesting(
      ConvertToDefaultUseCases(source_params),
      ConvertToDefaultUseCases(destination_params), experiment_tags);
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
          source_params, destination_params, {}),
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
          source_params, destination_params, {}),
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
  for (const auto& [site, param_map] : source_map) {
    for (const auto& [use_case, params] : param_map) {
      AddClassification(classifications.add_classifications(), site,
                        FilterClassification_SiteRole_SOURCE,
                        FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                        params, {use_case}, {DEFAULT_TAG});
    }
  }
  for (const auto& [site, param_map] : dest_map) {
    for (const auto& [use_case, params] : param_map) {
      AddClassification(classifications.add_classifications(), site,
                        FilterClassification_SiteRole_DESTINATION,
                        FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                        params, {use_case}, {DEFAULT_TAG});
    }
  }
  return classifications;
}

FilterClassifications MakeClassificationsProtoFromMap(
    const std::map<ClassificationMapKey, std::vector<std::string>>& map) {
  url_param_filter::FilterClassifications classifications;
  for (const auto& [key, params] : map) {
    AddClassification(classifications.add_classifications(), key.site,
                      key.site_role, key.site_match_type, params,
                      {
                          FilterClassification::USE_CASE_UNKNOWN,
                      },
                      {DEFAULT_TAG});
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
                      FilterClassification_SiteRole_SOURCE,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      params, use_cases, {});
  }
  for (const auto& [site, params] : dest_map) {
    AddClassification(classifications.add_classifications(), site,
                      FilterClassification_SiteRole_DESTINATION,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      params, use_cases, {});
  }
  return classifications;
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params) {
  return MakeFilterClassification(site, role, site_match_type, params, {},
                                  DEFAULT_TAG);
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases) {
  return MakeFilterClassification(site, role, site_match_type, params,
                                  use_cases, DEFAULT_TAG);
}

FilterClassification MakeFilterClassification(
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases,
    const std::string& experiment_identifier) {
  FilterClassification fc;
  AddClassification(&fc, site, role, site_match_type, params, use_cases,
                    {experiment_identifier});
  return fc;
}

void AddClassification(
    FilterClassification* classification,
    const std::string& site,
    FilterClassification_SiteRole role,
    FilterClassification_SiteMatchType site_match_type,
    const std::vector<std::string>& params,
    const std::vector<FilterClassification::UseCase>& use_cases,
    const std::vector<std::string>& experiment_tags) {
  classification->set_site(site);
  classification->set_site_role(role);
  classification->set_site_match_type(site_match_type);
  // The proto distinguishes between empty and not set; for the purposes of this
  // test helper, we avoid empty being considered an experiment.
  if (!experiment_tags.empty()) {
    for (auto i : experiment_tags) {
      classification->add_experiment_tags(i);
    }
  } else {
    classification->add_experiment_tags(DEFAULT_TAG);
  }
  for (const FilterClassification::UseCase& use_case : use_cases) {
    classification->add_use_cases(use_case);
  }
  for (const std::string& param : params) {
    FilterParameter* parameters = classification->add_parameters();
    parameters->set_name(param);
  }
}

}  // namespace url_param_filter
