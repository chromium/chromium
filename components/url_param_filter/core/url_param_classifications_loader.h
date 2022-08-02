// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_
#define COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_

#include <functional>
#include <string>
#include <unordered_map>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url_param_filter {

enum ClassificationExperimentStatus { EXPERIMENTAL, NON_EXPERIMENTAL };

// Struct used as key in map of classifications.
// Pair of a site's role and and the site name, e.g. (SOURCE, source.xyz).
struct ClassificationMapKey {
  FilterClassification::SiteRole site_role;
  FilterClassification::SiteMatchType site_match_type;
  std::string site;
};

bool operator==(const ClassificationMapKey& lhs,
                const ClassificationMapKey& rhs);

// Defined so that this can be used to key `std::map` as well as
// `std::unordered_map`
bool operator<(const ClassificationMapKey& lhs,
               const ClassificationMapKey& rhs);

struct ClassificationMapKeyHash {
  size_t operator()(const ClassificationMapKey& key) const {
    return std::hash<int>()(key.site_role) ^
           std::hash<std::string>()(key.site) ^
           std::hash<int>()(key.site_match_type);
  }
};

inline ClassificationMapKey SourceKey(std::string site) {
  return {
      .site_role =
          FilterClassification::SiteRole::FilterClassification_SiteRole_SOURCE,
      .site_match_type = FilterClassification::SiteMatchType::
          FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
      .site = site};
}

inline ClassificationMapKey DestinationKey(std::string site) {
  return {.site_role = FilterClassification::SiteRole::
              FilterClassification_SiteRole_DESTINATION,
          .site_match_type = FilterClassification::SiteMatchType::
              FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
          .site = site};
}

inline ClassificationMapKey SourceWildcardKey(std::string site_no_etld) {
  return {
      .site_role =
          FilterClassification::SiteRole::FilterClassification_SiteRole_SOURCE,
      .site_match_type = FilterClassification::SiteMatchType::
          FilterClassification_SiteMatchType_ETLD_WILDCARD,
      .site = site_no_etld};
}

// `unordered_map` is used for the outer map of (role, domain) pairs, which
// is likely to have hundreds. `map` is used for the inner map of `UseCase`,
// which will have a single digit number of keys.
using ClassificationMap = std::unordered_map<
    ClassificationMapKey,
    std::map<FilterClassification::UseCase,
             std::map<std::string, ClassificationExperimentStatus>>,
    ClassificationMapKeyHash>;

class ClassificationsLoader {
 public:
  static ClassificationsLoader* GetInstance();

  ClassificationsLoader(const ClassificationsLoader&) = delete;
  ClassificationsLoader& operator=(const ClassificationsLoader&) = delete;

  // Returns a mapping from site to all of its classifications.
  ClassificationMap GetClassifications();

  // Deserializes the proto from |raw_classifications|. The classifications that
  // are being read will have already been validated in the VerifyInstallation
  // method in our ComponentInstaller, so we can assume this input is valid.
  //
  // The component_source_classifications_ and
  // component_destination_classifications_ data members are populated by this
  // method if the proto is deserialized successfully.
  void ReadClassifications(const std::string& raw_classifications);

  // Resets the stored classification lists for testing.
  void ResetListsForTesting();

 private:
  friend class base::NoDestructor<ClassificationsLoader>;

  ClassificationsLoader();
  ~ClassificationsLoader();

  // Creates a mapping from a site to its `role` classifications by retrieving
  // classifications from either the Component Updater or the feature flag.
  // If classifications from both are provided, then the feature flag
  // classifications take precedence.
  ClassificationMap GetClassificationsInternal();

  absl::optional<ClassificationMap> component_classifications_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_param_filter
#endif  // COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_
