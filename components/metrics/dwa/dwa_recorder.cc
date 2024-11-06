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

namespace metrics::dwa {

BASE_FEATURE(kDwaFeature, "DwaFeature", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Takes |raw_entries_metrics|, a vector of metric_hash to metric_value maps,
// and returns it as a vector of EntryMetrics as defined in
// deidentified_web_analytics.proto.
std::vector<::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics>
TransformEntriesMetrics(
    const std::vector<base::flat_map<uint64_t, int64_t>>& raw_entries_metrics) {
  std::vector<::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics>
      entries_metrics;
  entries_metrics.reserve(raw_entries_metrics.size());

  for (const auto& raw_entry_metrics : raw_entries_metrics) {
    ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics
        entry_metric;
    for (const auto& [metric_hash, metric_value] : raw_entry_metrics) {
      ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics::Metric*
          metric = entry_metric.add_metric();
      metric->set_name_hash(metric_hash);
      metric->set_value(metric_value);
    }
    entries_metrics.push_back(std::move(entry_metric));
  }

  return entries_metrics;
}

// Takes a vector of entries, aggregates them, and then returns a vector of
// dwa events. The contents of |entries| are moved in this function and
// should not be used after.
std::vector<::dwa::DeidentifiedWebAnalyticsEvent> BuildDwaEvents(
    const std::vector<::metrics::dwa::mojom::DwaEntryPtr>& entries) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::unordered_map<std::string, std::string> active_field_trial_groups;

  for (const auto& active_group : active_groups) {
    active_field_trial_groups.insert(
        std::make_pair(active_group.trial_name, active_group.group_name));
  }

  // Maps {event_hash: {content_hash: vector<metrics>}}.
  std::unordered_map<
      uint64_t, std::unordered_map<
                    uint64_t, std::vector<base::flat_map<uint64_t, int64_t>>>>
      dwa_events_aggregation;
  // Maps {event_hash: vector<field_trials>}.
  std::unordered_map<uint64_t, base::flat_map<std::string, bool>>
      dwa_events_field_trials;

  for (const auto& entry : entries) {
    dwa_events_aggregation[entry->event_hash][entry->content_hash].push_back(
        std::move(entry->metrics));
    dwa_events_field_trials.try_emplace(entry->event_hash,
                                        std::move(entry->studies_of_interest));
  }

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events;

  for (const auto& [event_hash, content_and_metrics] : dwa_events_aggregation) {
    ::dwa::DeidentifiedWebAnalyticsEvent event;

    event.set_event_hash(event_hash);
    for (const auto& [content_hash, raw_entries_metrics] :
         content_and_metrics) {
      ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric* content_metric =
          event.add_content_metrics();
      content_metric->set_content_type(::dwa::DeidentifiedWebAnalyticsEvent::
                                           ContentMetric::CONTENT_TYPE_URL);
      content_metric->set_content_hash(content_hash);

      std::vector<
          ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics>
          entries_metrics = TransformEntriesMetrics(raw_entries_metrics);
      content_metric->mutable_metrics()->Add(
          std::make_move_iterator(entries_metrics.begin()),
          std::make_move_iterator(entries_metrics.end()));
    }

    PopulateFieldTrialsForDwaEvent(dwa_events_field_trials[event_hash],
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
  recorder_enabled_ = base::FeatureList::IsEnabled(kDwaFeature);
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

bool DwaRecorder::HasPageLoadEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !page_load_events_.empty();
}

}  // namespace metrics::dwa
