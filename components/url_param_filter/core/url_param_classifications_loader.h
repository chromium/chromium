// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_
#define COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_

#include <unordered_map>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url_param_filter {

enum ClassificationExperimentStatus { EXPERIMENTAL, NON_EXPERIMENTAL };

// `unordered_map` is used for the outer map of domains, which is likely to have
// hundreds. `map` is used for the inner map of `UseCase`, which will have a
// single digit number of keys.
using ClassificationMap = std::unordered_map<
    std::string,
    std::map<FilterClassification::UseCase,
             std::map<std::string, ClassificationExperimentStatus>>>;

class ClassificationsLoader {
 public:
  static ClassificationsLoader* GetInstance();

  ClassificationsLoader(const ClassificationsLoader&) = delete;
  ClassificationsLoader& operator=(const ClassificationsLoader&) = delete;

  // Returns a mapping from site to its source classifications.
  ClassificationMap GetSourceClassifications();

  // Returns a mapping from site to its destination classifications.
  ClassificationMap GetDestinationClassifications();

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
  ClassificationMap GetClassificationsInternal(
      FilterClassification_SiteRole role);

  absl::optional<ClassificationMap> component_source_classification_map_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;
  absl::optional<ClassificationMap> component_destination_classification_map_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_param_filter
#endif  // COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_CLASSIFICATIONS_LOADER_H_
