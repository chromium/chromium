// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UI_FORM_FACTOR_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_UI_FORM_FACTOR_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

// Provides metrics related to the device's form factor.
class FormFactorMetricsProvider : public MetricsProvider {
 public:
  FormFactorMetricsProvider() = default;

  FormFactorMetricsProvider(const FormFactorMetricsProvider&) = delete;
  FormFactorMetricsProvider& operator=(const FormFactorMetricsProvider&) =
      delete;

  ~FormFactorMetricsProvider() override = default;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 protected:
  // Returns the device's form factor. Protected and virtual for testing.
  virtual SystemProfileProto::Hardware::FormFactor GetFormFactor() const;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UI_FORM_FACTOR_METRICS_PROVIDER_H_
