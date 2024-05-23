// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
#define COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"

namespace variations {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Exposed for testing.
enum class InvalidStudyReason {
  kInvalidMinVersion = 0,
  kInvalidMaxVersion = 1,
  kInvalidMinOsVersion = 2,
  kInvalidMaxOsVersion = 3,
  kMissingExperimentName = 4,
  kRepeatedExperimentName = 5,
  kTotalProbabilityOverflow = 6,
  kMissingDefaultExperimentInList = 7,
  kBlankStudyName = 8,
  kExperimentProbabilityOverflow = 9,
  kTriggerAndNonTriggerExperimentId = 10,
  kInvalidStudyName = 11,
  kInvalidExperimentName = 12,
  kInvalidFeatureName = 13,
  kInvalidForcingFlag = 14,
  kMaxValue = kInvalidForcingFlag,
};

class Study;

// Wrapper over Study with extra information computed during pre-processing,
// such as whether the study is expired and its total probability.
class COMPONENT_EXPORT(VARIATIONS) ProcessedStudy {
 public:
  // The default group used when a study doesn't specify one. This is needed
  // because the field trial api requires a default group name.
  static const char kGenericDefaultExperimentName[];

  ProcessedStudy();
  ProcessedStudy(const ProcessedStudy& other);
  ~ProcessedStudy();

  bool Init(const Study* study);

  const Study* study() const { return study_; }

  base::FieldTrial::Probability total_probability() const {
    return total_probability_;
  }

  bool all_assignments_to_one_group() const {
    return all_assignments_to_one_group_;
  }

  // Gets the index of the experiment with the given |name|. Returns -1 if no
  // experiment is found.
  int GetExperimentIndexByName(const std::string& name) const;

  // Gets the default experiment name for the study, or a generic one if none is
  // specified.
  const std::string_view GetDefaultExperimentName() const;

 private:
  // Corresponding Study object. Weak reference.
  raw_ptr<const Study> study_ = nullptr;

  // Computed total group probability for the study.
  base::FieldTrial::Probability total_probability_ = 0;

  // Whether all assignments are to a single group.
  bool all_assignments_to_one_group_ = false;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
