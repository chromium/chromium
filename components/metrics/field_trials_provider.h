// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_
#define COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

// TODO(crbug.com/41187035): Once MetricsProvider/SystemProfileProto are moved
// into
// //services/metrics, then //components/variations can depend on them, and
// this should be moved there.
namespace variations {

class SyntheticTrialRegistry;
struct ActiveGroupId;

class FieldTrialsProvider : public metrics::MetricsProvider {
 public:
  // |registry| must outlive this metrics provider.
  FieldTrialsProvider(SyntheticTrialRegistry* registry,
                      std::string_view suffix);

  FieldTrialsProvider(const FieldTrialsProvider&) = delete;
  FieldTrialsProvider& operator=(const FieldTrialsProvider&) = delete;

  ~FieldTrialsProvider() override;

  // metrics::MetricsProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks log_creation_time,
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  // Sets |log_creation_time_| to |time|.
  void SetLogCreationTimeForTesting(base::TimeTicks time);

 private:
  // Populates |field_trial_ids| with currently active field trials groups. The
  // trial and group names are suffixed with |suffix_| before being hashed.
  void GetFieldTrialIds(std::vector<ActiveGroupId>* field_trial_ids) const;

  // Gets active FieldTrials and SyntheticFieldTrials and populates
  // |system_profile_proto| with them.
  void GetAndWriteFieldTrials(
      metrics::SystemProfileProto* system_profile_proto) const;

  // The most recent time passed to
  // ProvideSystemProfileMetricsWithLogCreationTime().
  base::TimeTicks log_creation_time_;

  raw_ptr<SyntheticTrialRegistry> registry_;

  // Suffix used for the field trial names before they are hashed for uploads.
  std::string suffix_;
};

}  // namespace variations

#endif  // COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_
