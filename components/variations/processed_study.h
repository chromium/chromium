// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
#define COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"

namespace variations {

class Study;

// Wrapper over Study with extra information computed during pre-processing,
// such as whether the study is expired and its total probability.
class ProcessedStudy {
 public:
  // The default group used when a study doesn't specify one. This is needed
  // because the field trial api requires a default group name.
  static const char kGenericDefaultExperimentName[];

  ProcessedStudy();
  ProcessedStudy(const ProcessedStudy& other);
  ~ProcessedStudy();

  bool Init(const Study* study, bool is_expired);

  const Study* study() const { return study_; }

  base::FieldTrial::Probability total_probability() const {
    return total_probability_;
  }

  bool all_assignments_to_one_group() const {
    return all_assignments_to_one_group_;
  }

  bool is_expired() const { return is_expired_; }

  const std::vector<std::string>& associated_features() const {
    return associated_features_;
  }

  // Gets the index of the experiment with the given |name|. Returns -1 if no
  // experiment is found.
  int GetExperimentIndexByName(const std::string& name) const;

  // Gets the default experiment name for the study, or a generic one if none is
  // specified.
  const char* GetDefaultExperimentName() const;

  static bool ValidateAndAppendStudy(
      const Study* study,
      bool is_expired,
      std::vector<ProcessedStudy>* processed_studies);

 private:
  // Corresponding Study object. Weak reference.
  const Study* study_ = nullptr;

  // Computed total group probability for the study.
  base::FieldTrial::Probability total_probability_ = 0;

  // Whether all assignments are to a single group.
  bool all_assignments_to_one_group_ = false;

  // Whether the study is expired.
  bool is_expired_ = false;

  // A list of feature names associated with this study by default. Studies
  // might have groups that do not specify any feature associations – this is
  // often the case for a default group, for example. The features listed here
  // will be associated with all such groups.
  std::vector<std::string> associated_features_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
