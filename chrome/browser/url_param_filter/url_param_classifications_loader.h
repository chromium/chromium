// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_

#include <unordered_map>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url_param_filter {

using ClassificationMap =
    std::unordered_map<std::string, url_param_filter::FilterClassification>;

class ClassificationsLoader {
 public:
  static ClassificationsLoader* GetInstance();

  ClassificationsLoader(const ClassificationsLoader&) = delete;
  ClassificationsLoader& operator=(const ClassificationsLoader&) = delete;

  // Returns a mapping from site to it's source classifications.
  // These classifications are retrieved from either the Component Updater or
  // the feature flag. If classifications from both sources are provided, then
  // the feature flag takes precedence.
  ClassificationMap GetSourceClassifications();

  // Returns a mapping from site to it's destination classifications.
  // These classifications are retrieved from either the Component Updater or
  // the feature flag. If classifications from both sources are provided, then
  // the feature flag takes precedence.
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

  absl::optional<std::vector<FilterClassification>>
      component_source_classifications_ GUARDED_BY_CONTEXT(sequence_checker_);
  absl::optional<std::vector<FilterClassification>>
      component_destination_classifications_
          GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_
