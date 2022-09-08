// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <cstdint>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"

namespace variations {
namespace {

void LogInvalidReason(InvalidStudyReason reason) {
  base::UmaHistogramEnumeration("Variations.InvalidStudyReason", reason);
}

// Validates the sanity of |study| and computes the total probability and
// whether all assignments are to a single group.
bool ValidateStudyAndComputeTotalProbability(
    const Study& study,
    base::FieldTrial::Probability* total_probability,
    bool* all_assignments_to_one_group,
    std::vector<std::string>* associated_features) {
  if (study.name().empty()) {
    LogInvalidReason(InvalidStudyReason::kBlankStudyName);
    DVLOG(1) << "study with missing study name";
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

  const std::string& default_group_name = study.default_experiment_name();
  base::FieldTrial::Probability divisor = 0;

  bool multiple_assigned_groups = false;
  bool found_default_group = false;

  std::set<std::string> experiment_names;
  std::set<std::string> features_to_associate;

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.name().empty()) {
      LogInvalidReason(InvalidStudyReason::kMissingExperimentName);
      DVLOG(1) << study.name() << " is missing experiment " << i << " name";
      return false;
    }
    if (!experiment_names.insert(experiment.name()).second) {
      LogInvalidReason(InvalidStudyReason::kRepeatedExperimentName);
      DVLOG(1) << study.name() << " has a repeated experiment name "
               << study.experiment(i).name();
      return false;
    }

    // Note: This checks for ACTIVATE_ON_QUERY, since there is no reason to
    // have this association with ACTIVATE_ON_STARTUP (where the trial starts
    // active), as well as allowing flexibility to disable this behavior in the
    // future from the server by introducing a new activation type.
    if (study.activation_type() == Study_ActivationType_ACTIVATE_ON_QUERY) {
      const auto& features = experiment.feature_association();
      for (int j = 0; j < features.enable_feature_size(); ++j) {
        features_to_associate.insert(features.enable_feature(j));
      }
      for (int j = 0; j < features.disable_feature_size(); ++j) {
        features_to_associate.insert(features.disable_feature(j));
      }
    }

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
    if (study.experiment(i).name() == default_group_name)
      found_default_group = true;
  }

  // Specifying a default experiment is optional, so finding it in the
  // experiment list is only required when it is specified.
  if (!study.default_experiment_name().empty() && !found_default_group) {
    LogInvalidReason(InvalidStudyReason::kMissingDefaultExperimentInList);
    DVLOG(1) << study.name() << " is missing default experiment ("
             << study.default_experiment_name() << ") in its experiment list";
    // The default group was not found in the list of groups. This study is not
    // valid.
    return false;
  }

  // Ensure that groups that don't explicitly enable/disable any features get
  // associated with all features in the study (i.e. so "Default" group gets
  // reported).
  if (!features_to_associate.empty()) {
    associated_features->insert(associated_features->end(),
                                features_to_associate.begin(),
                                features_to_associate.end());
  }

  *total_probability = divisor;
  *all_assignments_to_one_group = !multiple_assigned_groups;
  return true;
}

}  // namespace

// static
const char ProcessedStudy::kGenericDefaultExperimentName[] =
    "VariationsDefaultExperiment";

ProcessedStudy::ProcessedStudy() {}

ProcessedStudy::ProcessedStudy(const ProcessedStudy& other) = default;

ProcessedStudy::~ProcessedStudy() = default;

bool ProcessedStudy::Init(const Study* study, bool is_expired) {
  base::FieldTrial::Probability total_probability = 0;
  bool all_assignments_to_one_group = false;
  std::vector<std::string> associated_features;
  if (!ValidateStudyAndComputeTotalProbability(*study, &total_probability,
                                               &all_assignments_to_one_group,
                                               &associated_features)) {
    return false;
  }

  study_ = study;
  is_expired_ = is_expired;
  total_probability_ = total_probability;
  all_assignments_to_one_group_ = all_assignments_to_one_group;
  associated_features_.swap(associated_features);
  return true;
}

int ProcessedStudy::GetExperimentIndexByName(const std::string& name) const {
  for (int i = 0; i < study_->experiment_size(); ++i) {
    if (study_->experiment(i).name() == name)
      return i;
  }

  return -1;
}

const base::StringPiece ProcessedStudy::GetDefaultExperimentName() const {
  if (study_->default_experiment_name().empty())
    return kGenericDefaultExperimentName;

  return study_->default_experiment_name();
}

}  // namespace variations
