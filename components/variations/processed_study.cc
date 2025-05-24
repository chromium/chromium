// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "entropy_provider.h"
#include "variations_layers.h"

namespace variations {
namespace {

void LogInvalidReason(InvalidStudyReason reason) {
  base::UmaHistogramEnumeration("Variations.InvalidStudyReason", reason);
}

// TODO(crbug.com/41449497): Use
// base::FeatureList::IsValidFeatureOrFieldTrialName once WebRTC trials with ","
// in group names are removed.
bool IsValidExperimentName(const std::string& name) {
  return base::IsStringASCII(name) &&
         name.find_first_of("<*") == std::string::npos;
}

// Validates the sanity of |study| and computes the total probability and
// whether all assignments are to a single group.
bool ValidateStudyAndComputeTotalProbability(
    const Study& study,
    base::FieldTrial::Probability* total_probability,
    bool* all_assignments_to_one_group) {
  if (study.name().empty()) {
    LogInvalidReason(InvalidStudyReason::kBlankStudyName);
    DVLOG(1) << "study with missing study name";
    return false;
  }

  if (!base::FeatureList::IsValidFeatureOrFieldTrialName(study.name())) {
    LogInvalidReason(InvalidStudyReason::kInvalidStudyName);
    DVLOG(1) << study.name() << " is an invalid experiment name";
    return false;
  }

  if (study.filter().has_min_version() &&
      !base::Version::IsValidWildcardString(study.filter().min_version())) {
    LogInvalidReason(InvalidStudyReason::kInvalidMinVersion);
    DVLOG(1) << study.name()
             << " has invalid min version: " << study.filter().min_version();
    return false;
  }
  if (study.filter().has_max_version() &&
      !base::Version::IsValidWildcardString(study.filter().max_version())) {
    LogInvalidReason(InvalidStudyReason::kInvalidMaxVersion);
    DVLOG(1) << study.name()
             << " has invalid max version: " << study.filter().max_version();
    return false;
  }
  if (study.filter().has_min_os_version() &&
      !base::Version::IsValidWildcardString(study.filter().min_os_version())) {
    LogInvalidReason(InvalidStudyReason::kInvalidMinOsVersion);
    DVLOG(1) << study.name() << " has invalid min os version: "
             << study.filter().min_os_version();
    return false;
  }
  if (study.filter().has_max_os_version() &&
      !base::Version::IsValidWildcardString(study.filter().max_os_version())) {
    LogInvalidReason(InvalidStudyReason::kInvalidMaxOsVersion);
    DVLOG(1) << study.name() << " has invalid max os version: "
             << study.filter().max_os_version();
    return false;
  }

  // Validate experiment names
  {
    std::set<std::string> experiment_names;
    for (const auto& experiment : study.experiment()) {
      if (experiment.name().empty()) {
        LogInvalidReason(InvalidStudyReason::kMissingExperimentName);
        DVLOG(1) << study.name() << " is missing an experiment name";
        return false;
      }
      if (!IsValidExperimentName(experiment.name())) {
        LogInvalidReason(InvalidStudyReason::kInvalidExperimentName);
        DVLOG(1) << study.name() << " has a invalid experiment name "
                 << experiment.name();
        return false;
      }
      if (!experiment_names.insert(experiment.name()).second) {
        LogInvalidReason(InvalidStudyReason::kRepeatedExperimentName);
        DVLOG(1) << study.name() << " has a repeated experiment name "
                 << experiment.name();
        return false;
      }
    }

    // Specifying a default experiment is optional, so finding it in the
    // experiment list is only required when it is specified.
    if (!study.default_experiment_name().empty() &&
        !base::Contains(experiment_names, study.default_experiment_name())) {
      LogInvalidReason(InvalidStudyReason::kMissingDefaultExperimentInList);
      DVLOG(1) << study.name() << " is missing default experiment ("
               << study.default_experiment_name() << ") in its experiment list";
      // The default group was not found in the list of groups. This study is
      // not valid.
      return false;
    }
  }

  for (const auto& experiment : study.experiment()) {
    if (!experiment.has_feature_association())
      continue;
    for (const auto& feature :
         experiment.feature_association().enable_feature()) {
      if (!base::FeatureList::IsValidFeatureOrFieldTrialName(feature)) {
        LogInvalidReason(InvalidStudyReason::kInvalidFeatureName);
        DVLOG(1) << study.name() << " has a feature experiment name "
                 << feature;
        return false;
      }
    }
    for (const auto& feature :
         experiment.feature_association().disable_feature()) {
      if (!base::FeatureList::IsValidFeatureOrFieldTrialName(feature)) {
        LogInvalidReason(InvalidStudyReason::kInvalidFeatureName);
        DVLOG(1) << study.name() << " has a feature experiment name "
                 << feature;
        return false;
      }
    }
    if (!base::FeatureList::IsValidFeatureOrFieldTrialName(
            experiment.feature_association().forcing_feature_on())) {
      LogInvalidReason(InvalidStudyReason::kInvalidFeatureName);
      DVLOG(1) << study.name() << " has a feature experiment name "
               << experiment.feature_association().forcing_feature_on();
      return false;
    }
    if (!base::FeatureList::IsValidFeatureOrFieldTrialName(
            experiment.feature_association().forcing_feature_off())) {
      LogInvalidReason(InvalidStudyReason::kInvalidFeatureName);
      DVLOG(1) << study.name() << " has a feature experiment name "
               << experiment.feature_association().forcing_feature_off();
      return false;
    }
  }

  for (const auto& experiment : study.experiment()) {
    const auto& flag = experiment.forcing_flag();
    // Forcing flags are passed to CommandLine::HasSwitch. It should be safe to
    // pass invalid flags to that, but we do validation to prevent triggering
    // the DCHECK there.
    if (base::ToLowerASCII(flag) != flag) {
      LogInvalidReason(InvalidStudyReason::kInvalidForcingFlag);
      DVLOG(1) << study.name() << " has an invalid forcing flag " << flag;
      return false;
    }
  }

  base::FieldTrial::Probability divisor = 0;
  bool multiple_assigned_groups = false;

  for (const auto& experiment : study.experiment()) {
    if (experiment.has_google_web_experiment_id() &&
        experiment.has_google_web_trigger_experiment_id()) {
      LogInvalidReason(InvalidStudyReason::kTriggerAndNonTriggerExperimentId);
      DVLOG(1) << study.name() << " has experiment (" << experiment.name()
               << ") with a google_web_experiment_id and a "
               << "web_trigger_experiment_id.";
      return false;
    }

    if (!experiment.has_forcing_flag() && experiment.probability_weight() > 0) {
      // If |divisor| is not 0, there was at least one prior non-zero group.
      if (divisor != 0)
        multiple_assigned_groups = true;

      if (experiment.probability_weight() >
          std::numeric_limits<base::FieldTrial::Probability>::max()) {
        LogInvalidReason(InvalidStudyReason::kExperimentProbabilityOverflow);
        DVLOG(1) << study.name() << " has an experiment (" << experiment.name()
                 << ") with a probability weight of "
                 << experiment.probability_weight()
                 << " that exceeds the maximum supported value";
        return false;
      }

      if (divisor + experiment.probability_weight() >
          std::numeric_limits<base::FieldTrial::Probability>::max()) {
        LogInvalidReason(InvalidStudyReason::kTotalProbabilityOverflow);
        DVLOG(1) << study.name() << "'s total probability weight exceeds the "
                 << "maximum supported value";
        return false;
      }
      divisor += experiment.probability_weight();
    }
  }

  *total_probability = divisor;
  *all_assignments_to_one_group = !multiple_assigned_groups;
  return true;
}

}  // namespace

// static
const char ProcessedStudy::kGenericDefaultExperimentName[] =
    "VariationsDefaultExperiment";

ProcessedStudy::ProcessedStudy() = default;

ProcessedStudy::ProcessedStudy(const ProcessedStudy& other) = default;

ProcessedStudy::~ProcessedStudy() = default;

bool ProcessedStudy::Init(const Study* study) {
  base::FieldTrial::Probability total_probability = 0;
  bool all_assignments_to_one_group = false;
  std::vector<std::string> associated_features;
  if (!ValidateStudyAndComputeTotalProbability(*study, &total_probability,
                                               &all_assignments_to_one_group)) {
    return false;
  }

  study_ = study;
  total_probability_ = total_probability;
  all_assignments_to_one_group_ = all_assignments_to_one_group;
  return true;
}

int ProcessedStudy::GetExperimentIndexByName(const std::string& name) const {
  for (int i = 0; i < study_->experiment_size(); ++i) {
    if (study_->experiment(i).name() == name)
      return i;
  }

  return -1;
}

const std::string_view ProcessedStudy::GetDefaultExperimentName() const {
  if (study_->default_experiment_name().empty())
    return kGenericDefaultExperimentName;

  return study_->default_experiment_name();
}

}  // namespace variations
