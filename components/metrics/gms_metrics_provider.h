// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_GMS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_GMS_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// GmsMetricsProvider provides metrics related to Google Mobile
// Service like the current version installed on the device. Note that this
// class is currently only used on Android.
class GmsMetricsProvider : public metrics::MetricsProvider {
 public:
  GmsMetricsProvider();

  GmsMetricsProvider(const GmsMetricsProvider&) = delete;
  GmsMetricsProvider& operator=(const GmsMetricsProvider&) = delete;

  ~GmsMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;

 protected:
  virtual std::string GetGMSVersion();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_GMS_METRICS_PROVIDER_H_
