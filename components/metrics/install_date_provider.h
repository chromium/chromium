// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_INSTALL_DATE_PROVIDER_H_
#define COMPONENTS_METRICS_INSTALL_DATE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class PrefService;

// NOTE: This Provider is unfortunately entwined with the MetricsStateManager.
// The MetricsStateManager actually controls the state of the pref which keeps
// track of the install state, and also will set it.
// Since the MetricsStateManager does quite a bit of other work, and it is
// complex to disentangle, this provider is available if we just want the
// install_date to be set (currently for UKM).
// This means this Provider is *NOT* used in UMA.
// In the longer term, we should refactor MetricsStateManager such that
// the parts that are eligible for UKM can be reused.
namespace metrics {

// Provides the install date.
class InstallDateProvider : public MetricsProvider {
 public:
  explicit InstallDateProvider(PrefService* local_state)
      : local_state_(local_state) {}

  InstallDateProvider(const InstallDateProvider&) = delete;
  InstallDateProvider& operator=(const InstallDateProvider&) = delete;

  ~InstallDateProvider() override = default;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

  raw_ptr<PrefService> local_state_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_INSTALL_DATE_PROVIDER_H_
