// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/url_param_classifications_loader.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

namespace {
constexpr char DEFAULT_TAG[] = "default";

bool HasExperimentTag(const FilterClassification& classification,
                      const std::string& tag) {
  // We expect this list to almost never exceed 2 items, making a loop
  // acceptable.
  for (auto i : classification.experiment_tags()) {
    if (i == tag) {
      return true;
    }
  }
  return false;
}

void AppendParams(ClassificationMap& map,
                  const FilterClassification& classification,
                  FilterClassification::UseCase use_case) {
  // If site_match_type is not set or is set to MATCH_TYPE_UNKNOWN, default
  // to the exact match on eTLD+1.
  FilterClassification::SiteMatchType site_match_type = FilterClassification::
      SiteMatchType::FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE;
  if (classification.has_site_match_type() &&
      classification.site_match_type() !=
          FilterClassification::SiteMatchType::
              FilterClassification_SiteMatchType_MATCH_TYPE_UNKNOWN) {
    site_match_type = classification.site_match_type();
  }
  for (const FilterParameter& param : classification.parameters()) {
    // Any non-matching experimental params have been discarded previously.
    // We retain whether the classification was experimental, however, to write
    // a separate metric when those classifications are used.
    ClassificationMapKey key = {.site_role = classification.site_role(),
                                .site_match_type = site_match_type,
                                .site = classification.site()};
    std::string param_name = base::ToLowerASCII(param.name());

    auto status = !classification.experiment_tags().empty() &&
                          !HasExperimentTag(classification, DEFAULT_TAG)
                      ? ClassificationExperimentStatus::EXPERIMENTAL
                      : ClassificationExperimentStatus::NON_EXPERIMENTAL;

    // Preserve existing entry in map if it is marked as NON_EXPERIMENTAL.
    if (map.find(key) != map.end() &&
        map[key].find(use_case) != map[key].end() &&
        map[key][use_case].find(param_name) != map[key][use_case].end() &&
        map[key][use_case][param_name] ==
            ClassificationExperimentStatus::NON_EXPERIMENTAL) {
      status = ClassificationExperimentStatus::NON_EXPERIMENTAL;
    }
    map[key][use_case][param_name] = status;
  }
}

void ProcessClassification(ClassificationMap& map,
                           const FilterClassification& classification) {
  if (classification.use_cases_size() > 0) {
    for (int use_case : classification.use_cases()) {
      AppendParams(map, classification,
                   static_cast<FilterClassification::UseCase>(use_case));
    }
  } else {
    AppendParams(map, classification, FilterClassification::USE_CASE_UNKNOWN);
  }
}

ClassificationMap GetClassificationsFromFeature(
    const std::string& feature_classifications) {
  FilterClassifications classifications;
  ClassificationMap map;
  std::string out;
  base::Base64Decode(feature_classifications, &out);
  std::string uncompressed;
  if (compression::GzipUncompress(out, &uncompressed)) {
    if (classifications.ParseFromString(uncompressed)) {
      for (auto i : classifications.classifications()) {
        // When retrieving classifications from the feature, we do not allow
        // additional experiment overrides.
        DCHECK(i.experiment_tags().empty());
        ProcessClassification(map, i);
      }
    }
  }
  return map;
}

ClassificationMap GetClassificationMap(
    const std::vector<FilterClassification>& classifications) {
  ClassificationMap map;
  for (const FilterClassification& classification : classifications) {
    ProcessClassification(map, classification);
  }
  return map;
}

}  // anonymous namespace

bool operator==(const ClassificationMapKey& lhs,
                const ClassificationMapKey& rhs) {
  return std::tie(lhs.site_role, lhs.site_match_type, lhs.site) ==
         std::tie(rhs.site_role, rhs.site_match_type, rhs.site);
}

bool operator<(const ClassificationMapKey& lhs,
               const ClassificationMapKey& rhs) {
  return std::tie(lhs.site_role, lhs.site_match_type, lhs.site) <
         std::tie(rhs.site_role, rhs.site_match_type, rhs.site);
}

// static
ClassificationsLoader* ClassificationsLoader::GetInstance() {
  static base::NoDestructor<ClassificationsLoader> instance;
  return instance.get();
}

ClassificationMap ClassificationsLoader::GetClassifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClassificationsInternal();
}

void ClassificationsLoader::ReadClassifications(
    const std::string& raw_classifications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FilterClassifications classification_list;
  if (!classification_list.ParseFromString(raw_classifications))
    return;

  std::vector<FilterClassification> classifications;
  int total_applicable_source_classifications = 0;
  int total_applicable_destination_classifications = 0;
  std::string experiment_identifier = base::GetFieldTrialParamValueByFeature(
      features::kIncognitoParamFilterEnabled, "experiment_identifier");
  // If there is no experiment identifier passed via the feature, then use the
  // classifications that are marked `default`.
  if (experiment_identifier.empty()) {
    experiment_identifier = DEFAULT_TAG;
  }
  for (const FilterClassification& fc : classification_list.classifications()) {
    DCHECK(fc.has_site());
    DCHECK(fc.has_site_role());
    if (!HasExperimentTag(fc, experiment_identifier)) {
      continue;
    }

    FilterClassification::SiteRole site_role = fc.site_role();
    if (site_role == FilterClassification_SiteRole_SOURCE) {
      classifications.push_back(fc);
      total_applicable_source_classifications++;
    }

    if (site_role == FilterClassification_SiteRole_DESTINATION) {
      classifications.push_back(fc);
      total_applicable_destination_classifications++;
    }
  }

  component_classifications_ = GetClassificationMap(classifications);
  base::UmaHistogramCounts10000(
      "Navigation.UrlParamFilter.ApplicableClassificationCount.Source",
      total_applicable_source_classifications);
  base::UmaHistogramCounts10000(
      "Navigation.UrlParamFilter.ApplicableClassificationCount.Destination",
      total_applicable_destination_classifications);
}

void ClassificationsLoader::ResetListsForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  component_classifications_.reset();
}

ClassificationsLoader::ClassificationsLoader() = default;
ClassificationsLoader::~ClassificationsLoader() = default;

ClassificationMap ClassificationsLoader::GetClassificationsInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Classifications from field trial params take precedence over the ones from
  // Component Updater.
  base::FieldTrialParams params;
  bool has_feature_params = base::GetFieldTrialParamsByFeature(
      features::kIncognitoParamFilterEnabled, &params);
  // Retrieve classifications from feature if provided as a parameter.
  if (has_feature_params && params.find("classifications") != params.end()) {
    return GetClassificationsFromFeature(
        params.find("classifications")->second);
  }

  // If no feature classifications are given, use the component-provided
  // classifications.
  return component_classifications_.has_value()
             ? component_classifications_.value()
             : ClassificationMap();
}

}  // namespace url_param_filter
