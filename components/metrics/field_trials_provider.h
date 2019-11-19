// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_
#define COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"

// TODO(crbug/507665): Once MetricsProvider/SystemProfileProto are moved into
// //services/metrics, then //components/variations can depend on them, and
// this should be moved there.
namespace variations {

class SyntheticTrialRegistry;
struct ActiveGroupId;

class FieldTrialsProvider : public metrics::MetricsProvider {
 public:
  // |registry| must outlive this metrics provider.
  FieldTrialsProvider(SyntheticTrialRegistry* registry,
                      base::StringPiece suffix);
  ~FieldTrialsProvider() override;

  // metrics::MetricsProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks log_creation_time,
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  // Overrideable for testing.
  virtual void GetFieldTrialIds(
      std::vector<ActiveGroupId>* field_trial_ids) const;

  SyntheticTrialRegistry* registry_;

  // Suffix used for the field trial names before they are hashed for uploads.
  std::string suffix_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialsProvider);
};

}  // namespace variations

#endif  // COMPONENTS_METRICS_FIELD_TRIALS_PROVIDER_H_
