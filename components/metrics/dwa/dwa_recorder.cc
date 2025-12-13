// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include <algorithm>
#include <numeric>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "components/metrics/private_metrics/private_metrics_features.h"

namespace metrics::dwa {

BASE_FEATURE(kDwaFeature, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Populates |dwa_event|.field_trials with the field trial/group name hashes
// for the field_trials we are interested in, |studies_of_interest|.
// |active_field_trial_groups| contains a mapping of field trial names to
// group names that we are currently part of.
void PopulateFieldTrialsForDwaEvent(
    const base::flat_map<std::string, bool>& studies_of_interest,
    const std::unordered_map<std::string, std::string>&
        active_field_trial_groups,
    ::dwa::DeidentifiedWebAnalyticsEvent& dwa_event) {
  // Determine the study groups that is part of |studies_of_interest| in the
  // current session. If we are in a study part of |study_of_interest| in the
  // current session, Hash the study and group names and populate a new repeated
  // field_trials in |dwa_event|.
  for (const auto& [trial_name, _] : studies_of_interest) {
    auto it = active_field_trial_groups.find(trial_name);
    if (it != active_field_trial_groups.end()) {
      const auto& group_name = it->second;
      ::metrics::SystemProfileProto::FieldTrial* field_trial =
          dwa_event.add_field_trials();
      field_trial->set_name_id(base::HashFieldTrialName(trial_name));
      field_trial->set_group_id(base::HashFieldTrialName(group_name));
    }
  }
}

// Takes a vector of `entries`, and then returns a vector of dwa events. The
// vector of `entries` should not be used after.
std::vector<::dwa::DeidentifiedWebAnalyticsEvent> BuildDwaEvents(
    const std::vector<::metrics::dwa::mojom::DwaEntryPtr>& entries) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::unordered_map<std::string, std::string> active_field_trial_groups;

  for (const auto& active_group : active_groups) {
    active_field_trial_groups.insert(
        std::make_pair(active_group.trial_name, active_group.group_name));
  }

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events;

  for (const auto& entry : entries) {
    ::dwa::DeidentifiedWebAnalyticsEvent event;

    event.set_event_hash(entry->event_hash);
    event.set_content_hash(entry->content_hash);

    for (const auto& [metric_hash, metric_value] : entry->metrics) {
      auto* dwa_event_metric = event.add_metric();
      dwa_event_metric->set_name_hash(metric_hash);
      dwa_event_metric->set_value(metric_value);
    }

    PopulateFieldTrialsForDwaEvent(entry->studies_of_interest,
                                   active_field_trial_groups, event);

    dwa_events.push_back(std::move(event));
  }

  return dwa_events;
}

}  // namespace

DwaRecorder::DwaRecorder() = default;

DwaRecorder::~DwaRecorder() = default;

void DwaRecorder::EnableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_enabled_ = IsDwaOrPrivateMetricsFeatureEnabled();
}

void DwaRecorder::DisableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_enabled_ = false;
}

void DwaRecorder::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
}

bool DwaRecorder::IsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return recorder_enabled_;
}

// static
DwaRecorder* DwaRecorder::Get() {
  static base::NoDestructor<DwaRecorder> recorder;
  return recorder.get();
}

void DwaRecorder::AddEntry(metrics::dwa::mojom::DwaEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recorder_enabled_) {
    return;
  }

  entries_.push_back(std::move(entry));
}

bool DwaRecorder::HasEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !entries_.empty();
}

std::vector<::dwa::DeidentifiedWebAnalyticsEvent> DwaRecorder::TakeDwaEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No entries, so there's nothing to do.
  if (entries_.empty()) {
    return std::vector<::dwa::DeidentifiedWebAnalyticsEvent>();
  }

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events =
      BuildDwaEvents(entries_);
  entries_.clear();

  return dwa_events;
}

const std::vector<metrics::dwa::mojom::DwaEntryPtr>&
DwaRecorder::GetEntriesForTesting() const {
  return entries_;
}

// static
bool DwaRecorder::IsDwaOrPrivateMetricsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDwaFeature) ||
         base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature);
}

}  // namespace metrics::dwa
