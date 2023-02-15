// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_experiments_translator.h"

#include <memory>
#include <string>
#include <vector>

#include "components/feed/feed_feature_list.h"

namespace feed {

absl::optional<Experiments> TranslateExperiments(
    const google::protobuf::RepeatedPtrField<feedwire::Experiment>&
        wire_experiments) {
  // Set up the Experiments map that contains the trial -> list of groups.
  absl::optional<Experiments> experiments = absl::nullopt;
  if (wire_experiments.size() > 0) {
    Experiments e;
    for (feedwire::Experiment exp : wire_experiments) {
      if (exp.has_trial_name() && exp.has_group_name()) {
        // Extract experiment in response that contains both trial and
        // group names.
        if (e.find(exp.trial_name()) != e.end()) {
          e[exp.trial_name()].push_back(exp.group_name());
        } else {
          e[exp.trial_name()] = {exp.group_name()};
        }
      } else if (exp.has_experiment_id() &&
                 base::FeatureList::IsEnabled(kFeedExperimentIDTagging)) {
        // Extract experiment in response that contains an experiment ID.
        std::string trial_name =
            exp.has_trial_name() ? exp.trial_name() : kDiscoverFeedExperiments;
        if (e.find(trial_name) != e.end()) {
          e[trial_name].push_back(exp.experiment_id());
        } else {
          e[trial_name] = {exp.experiment_id()};
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
