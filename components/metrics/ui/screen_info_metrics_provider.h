// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_

#include <optional>

#include "components/metrics/metrics_provider.h"
#include "ui/gfx/geometry/size.h"

namespace metrics {

// ScreenInfoMetricsProvider provides metrics related to screen info.
class ScreenInfoMetricsProvider : public MetricsProvider {
 public:
  ScreenInfoMetricsProvider();

  ScreenInfoMetricsProvider(const ScreenInfoMetricsProvider&) = delete;
  ScreenInfoMetricsProvider& operator=(const ScreenInfoMetricsProvider&) =
      delete;

  ~ScreenInfoMetricsProvider() override;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 protected:
  // Exposed for the sake of mocking in test code.

  // Returns the screen size for the primary monitor if available.
  virtual std::optional<gfx::Size> GetScreenSize() const;

  // Returns the device scale factor for the primary monitor.
  virtual float GetScreenDeviceScaleFactor() const;

  // Returns the number of monitors the user is using.
  virtual int GetScreenCount() const;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_
