// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"

namespace metrics::dwa {

DwaRecorder::DwaRecorder() = default;

DwaRecorder::~DwaRecorder() = default;

void DwaRecorder::EnableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/369573207): Check to see if DWA is enabled using feature flags
  // before enabling recorder.
  recorder_enabled_ = true;
}

void DwaRecorder::DisableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_enabled_ = false;
}

void DwaRecorder::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  page_load_events_.clear();
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

void DwaRecorder::OnPageLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recorder_enabled_) {
    return;
  }

  // No entries, so there's nothing to do.
  if (entries_.empty()) {
    return;
  }

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events =
      BuildDwaEvents(entries_);
  entries_.clear();

  if (dwa_events.empty()) {
    return;
  }

  // Puts existing |dwa_events_| into a page load event.
  ::dwa::PageLoadEvents page_load_event;
  page_load_event.mutable_events()->Add(
      std::make_move_iterator(dwa_events.begin()),
      std::make_move_iterator(dwa_events.end()));

  // Add the page load event to the list of page load events.
  page_load_events_.push_back(std::move(page_load_event));
}

std::vector<::dwa::PageLoadEvents> DwaRecorder::TakePageLoadEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<::dwa::PageLoadEvents> results = std::move(page_load_events_);
  page_load_events_.clear();
  return results;
}

// static
std::vector<::dwa::DeidentifiedWebAnalyticsEvent> DwaRecorder::BuildDwaEvents(
    const std::vector<::metrics::dwa::mojom::DwaEntryPtr>& entries) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::unordered_map<std::string, std::string> active_field_trial_groups;

  for (const auto& active_group : active_groups) {
    active_field_trial_groups.insert(
        std::make_pair(active_group.trial_name, active_group.group_name));
  }

  // TODO(b/369464150): Implement BuildDwaEvents
  return std::vector<::dwa::DeidentifiedWebAnalyticsEvent>();
}

// static
void DwaRecorder::PopulateFieldTrialsForDwaEvent(
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

}  // namespace metrics::dwa
