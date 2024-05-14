// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/field_trials_provider.h"

#include <string>
#include <string_view>
#include <vector>

#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace variations {

namespace {

void WriteFieldTrials(const std::vector<ActiveGroupId>& field_trial_ids,
                      metrics::SystemProfileProto* system_profile) {
  for (const ActiveGroupId& id : field_trial_ids) {
    metrics::SystemProfileProto::FieldTrial* field_trial =
        system_profile->add_field_trial();
    field_trial->set_name_id(id.name);
    field_trial->set_group_id(id.group);
  }
}

}  // namespace

FieldTrialsProvider::FieldTrialsProvider(SyntheticTrialRegistry* registry,
                                         std::string_view suffix)
    : registry_(registry), suffix_(suffix) {}
FieldTrialsProvider::~FieldTrialsProvider() = default;

void FieldTrialsProvider::GetFieldTrialIds(
    std::vector<ActiveGroupId>* field_trial_ids) const {
  // As the trial groups are included in metrics reports, we must not include
  // the low anonymity trials.
  variations::GetFieldTrialActiveGroupIds(suffix_, field_trial_ids);
}

void FieldTrialsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // ProvideSystemProfileMetricsWithLogCreationTime() should be called instead.
  NOTREACHED_IN_MIGRATION();
}

void FieldTrialsProvider::ProvideSystemProfileMetricsWithLogCreationTime(
    base::TimeTicks log_creation_time,
    metrics::SystemProfileProto* system_profile_proto) {
  // TODO(crbug.com/40697205): Maybe call ProvideCurrentSessionData() instead
  // from places in which ProvideSystemProfileMetricsWithLogCreationTime() is
  // called, e.g. startup_data.cc and background_tracing_metrics_provider.cc.

  log_creation_time_ = log_creation_time;

  const std::string& version = variations::GetSeedVersion();
  if (!version.empty())
    system_profile_proto->set_variations_seed_version(version);

  // TODO(crbug.com/40133600): Determine whether this can be deleted.
  GetAndWriteFieldTrials(system_profile_proto);
}

void FieldTrialsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // This function is called from both
  // ProvideSystemProfileMetricsWithLogCreationTime() and
  // ProvideCurrentSessionData() so that field trials activated in other metrics
  // providers are captured. We need both calls because in some scenarios in
  // which this class is used, only the former function gets called.
  DCHECK(!log_creation_time_.is_null());
  GetAndWriteFieldTrials(uma_proto->mutable_system_profile());
}

void FieldTrialsProvider::SetLogCreationTimeForTesting(base::TimeTicks time) {
  log_creation_time_ = time;
}

void FieldTrialsProvider::GetAndWriteFieldTrials(
    metrics::SystemProfileProto* system_profile_proto) const {
  system_profile_proto->clear_field_trial();

  std::vector<ActiveGroupId> field_trials;
  GetFieldTrialIds(&field_trials);
  WriteFieldTrials(field_trials, system_profile_proto);

  // May be null in tests.
  if (registry_) {
    std::vector<ActiveGroupId> synthetic_trials;
    registry_->GetSyntheticFieldTrialsOlderThan(log_creation_time_,
                                                &synthetic_trials, suffix_);
    WriteFieldTrials(synthetic_trials, system_profile_proto);
  }
}

}  // namespace variations
