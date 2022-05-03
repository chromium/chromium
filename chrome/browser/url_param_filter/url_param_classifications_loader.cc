// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "chrome/common/chrome_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

namespace {

ClassificationMap GetClassificationsFromFeature(
    const std::string& feature_classifications,
    FilterClassification_SiteRole role) {
  FilterClassifications classifications;
  ClassificationMap map;
  std::string out;
  base::Base64Decode(feature_classifications, &out);
  std::string uncompressed;
  if (compression::GzipUncompress(out, &uncompressed)) {
    if (classifications.ParseFromString(uncompressed)) {
      for (auto i : classifications.classifications()) {
        if (i.site_role() == role) {
          map[i.site()] = i;
        }
      }
    }
  }
  return map;
}

// If this is called before `ReadClassifications` has read classifications from
// the component, returns an empty map.
ClassificationMap GetClassificationsFromFile(
    const absl::optional<std::vector<FilterClassification>>& classifications) {
  if (!classifications.has_value())
    return ClassificationMap();
  ClassificationMap map;
  for (const FilterClassification& classification : classifications.value()) {
    map[classification.site()] = classification;
  }
  return map;
}

}  // anonymous namespace

// static
ClassificationsLoader* ClassificationsLoader::GetInstance() {
  static base::NoDestructor<ClassificationsLoader> instance;
  return instance.get();
}

ClassificationMap ClassificationsLoader::GetSourceClassifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClassificationsInternal(
      FilterClassification_SiteRole::FilterClassification_SiteRole_SOURCE);
}

ClassificationMap ClassificationsLoader::GetDestinationClassifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClassificationsInternal(
      FilterClassification_SiteRole::FilterClassification_SiteRole_DESTINATION);
}

void ClassificationsLoader::ReadClassifications(
    const std::string& raw_classifications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FilterClassifications classification_list;
  if (!classification_list.ParseFromString(raw_classifications))
    return;

  std::vector<FilterClassification> source_classifications,
      destination_classifications;
  for (const FilterClassification& fc : classification_list.classifications()) {
    DCHECK(fc.has_site());
    DCHECK(fc.has_site_role());
    if (fc.site_role() == FilterClassification_SiteRole_SOURCE)
      source_classifications.push_back(fc);

    if (fc.site_role() == FilterClassification_SiteRole_DESTINATION)
      destination_classifications.push_back(fc);
  }

  component_source_classifications_ = std::move(source_classifications);
  component_destination_classifications_ =
      std::move(destination_classifications);
}

void ClassificationsLoader::ResetListsForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  component_source_classifications_.reset();
  component_destination_classifications_.reset();
}

ClassificationsLoader::ClassificationsLoader() = default;
ClassificationsLoader::~ClassificationsLoader() = default;

ClassificationMap ClassificationsLoader::GetClassificationsInternal(
    FilterClassification_SiteRole role) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Classifications from field trial params take precedence over the ones from
  // Component Updater.
  base::FieldTrialParams params;
  bool has_feature_params = base::GetFieldTrialParamsByFeature(
      features::kIncognitoParamFilterEnabled, &params);
  // Retrieve classifications from feature if provided as a parameter.
  if (has_feature_params && params.find("classifications") != params.end()) {
    return GetClassificationsFromFeature(params.find("classifications")->second,
                                         role);
  }

  // If no feature classifications are given, use the component-provided
  // classifications.
  switch (role) {
    case FilterClassification_SiteRole::FilterClassification_SiteRole_SOURCE:
      return GetClassificationsFromFile(component_source_classifications_);
    case FilterClassification_SiteRole::
        FilterClassification_SiteRole_DESTINATION:
      return GetClassificationsFromFile(component_destination_classifications_);
    case FilterClassification_SiteRole_SITE_ROLE_UNKNOWN:
      return ClassificationMap();
  }
}

}  // namespace url_param_filter
