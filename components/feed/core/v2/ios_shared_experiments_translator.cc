// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_experiments_translator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/feed/feed_feature_list.h"

namespace feed {

bool ExperimentGroup::operator==(const ExperimentGroup& rhs) const {
  return name == rhs.name && experiment_id == rhs.experiment_id;
}

std::optional<Experiments> TranslateExperiments(
    const google::protobuf::RepeatedPtrField<feedwire::Experiment>&
        wire_experiments) {
  // Set up the Experiments map that contains the trial -> list of groups.
  std::optional<Experiments> experiments = std::nullopt;
  if (wire_experiments.size() > 0) {
    Experiments e;
    for (feedwire::Experiment exp : wire_experiments) {
      if (exp.has_trial_name() && exp.has_group_name()) {
        // Extract experiment in response that contains both trial and
        // group names.
        ExperimentGroup group;
        group.name = exp.group_name();
        group.experiment_id = 0;
        if (exp.has_experiment_id()) {
          base::StringToInt(exp.experiment_id(), &(group.experiment_id));
        }
        if (e.find(exp.trial_name()) != e.end()) {
          e[exp.trial_name()].push_back(group);
        } else {
          e[exp.trial_name()] = {group};
        }
      }
    }
    if (!e.empty()) {
      experiments = std::move(e);
    }
  }
  return experiments;
}

}  // namespace feed
