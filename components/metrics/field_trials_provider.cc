// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/field_trials_provider.h"

#include "base/strings/string_piece.h"
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
                                         base::StringPiece suffix)
    : registry_(registry), suffix_(suffix) {}
FieldTrialsProvider::~FieldTrialsProvider() = default;

void FieldTrialsProvider::GetFieldTrialIds(
    std::vector<ActiveGroupId>* field_trial_ids) const {
  // We use the default field trial suffixing (no suffix).
  variations::GetFieldTrialActiveGroupIds(suffix_, field_trial_ids);
}

void FieldTrialsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // ProvideSystemProfileMetricsWithLogCreationTime() should be called instead.
  NOTREACHED();
}

void FieldTrialsProvider::ProvideSystemProfileMetricsWithLogCreationTime(
    base::TimeTicks log_creation_time,
    metrics::SystemProfileProto* system_profile_proto) {
  std::vector<ActiveGroupId> field_trial_ids;
  const std::string& version = variations::GetSeedVersion();
  if (!version.empty())
    system_profile_proto->set_variations_seed_version(version);
  GetFieldTrialIds(&field_trial_ids);
  WriteFieldTrials(field_trial_ids, system_profile_proto);

  if (registry_) {
    std::vector<ActiveGroupId> synthetic_trials;
    registry_->GetSyntheticFieldTrialsOlderThan(log_creation_time,
                                                &synthetic_trials);
    WriteFieldTrials(synthetic_trials, system_profile_proto);
  }
}

}  // namespace variations
