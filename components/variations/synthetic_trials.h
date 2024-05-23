// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

namespace variations {

// Specifies when UMA reports should start being annotated with a synthetic
// field trial.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.variations
enum class SyntheticTrialAnnotationMode {
  // Start annotating UMA reports with this trial only after the next log opens.
  // The UMA report that will be generated from the log that is open at the time
  // of registration will not be annotated with this trial.
  kNextLog,
  // Start annotating UMA reports with this trial immediately, including the one
  // that will be generated from the log that is open at the time of
  // registration.
  kCurrentLog,
};

// A Field Trial and its selected group, which represent a particular
// Chrome configuration state. In other words, synthetic trials allow reporting
// some client state as if it were a field trial. For example, the trial name
// could map to a preference name, and the group name could map to a preference
// value.
class COMPONENT_EXPORT(VARIATIONS) SyntheticTrialGroup {
 public:
  SyntheticTrialGroup(std::string_view trial_name,
                      std::string_view group_name,
                      SyntheticTrialAnnotationMode annotation_mode);

  SyntheticTrialGroup(const SyntheticTrialGroup&);

  ~SyntheticTrialGroup() = default;

  base::FieldTrial::ActiveGroup active_group() const { return active_group_; }
  std::string_view trial_name() const { return active_group_.trial_name; }
  std::string_view group_name() const { return active_group_.group_name; }
  ActiveGroupId id() const { return id_; }
  base::TimeTicks start_time() const { return start_time_; }
  SyntheticTrialAnnotationMode annotation_mode() const {
    return annotation_mode_;
  }
  bool is_external() const { return is_external_; }

  void SetTrialName(std::string_view trial_name);
  void SetGroupName(std::string_view group_name);
  void SetStartTime(base::TimeTicks start_time) { start_time_ = start_time; }
  void SetAnnotationMode(SyntheticTrialAnnotationMode annotation_mode) {
    annotation_mode_ = annotation_mode;
  }
  void SetIsExternal(bool is_external) { is_external_ = is_external; }

 private:
  base::FieldTrial::ActiveGroup active_group_;
  ActiveGroupId id_;
  base::TimeTicks start_time_;

  // Determines when UMA reports should start being annotated with this trial
  // group.
  SyntheticTrialAnnotationMode annotation_mode_;

  // Whether this is an external experiment. I.e., if this synthetic trial was
  // registered through SyntheticTrialRegistry::RegisterExternalExperiments().
  // An example of an external experiment would be the Chrome updater randomly
  // assigning which binary to update to.
  bool is_external_ = false;
};

// Interface class to observe changes to synthetic trials in MetricsService.
class COMPONENT_EXPORT(VARIATIONS) SyntheticTrialObserver {
 public:
  // Called when the list of synthetic field trial groups has changed.
  // `trials_updated` contains a list of trials that were updated or added.
  // `trials_removed` are the field trials that are no longer active.
  // If there is an overlap of the trial names between the 2 lists, the
  // `trials_updated` contains the latest group.
  // `groups` contains the final list of all active groups.
  virtual void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& trials_updated,
      const std::vector<SyntheticTrialGroup>& trials_removed,
      const std::vector<SyntheticTrialGroup>& groups) = 0;

 protected:
  virtual ~SyntheticTrialObserver() = default;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
