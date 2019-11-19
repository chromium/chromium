// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/metrics/metrics_provider.h"
#include "ui/gfx/geometry/size.h"

namespace metrics {

// ScreenInfoMetricsProvider provides metrics related to screen info.
class ScreenInfoMetricsProvider : public MetricsProvider {
 public:
  ScreenInfoMetricsProvider();
  ~ScreenInfoMetricsProvider() override;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 protected:
  // Exposed for the sake of mocking in test code.

  // Returns the screen size for the primary monitor if available.
  virtual base::Optional<gfx::Size> GetScreenSize() const;

  // Returns the device scale factor for the primary monitor.
  virtual float GetScreenDeviceScaleFactor() const;

  // Returns the number of monitors the user is using.
  virtual int GetScreenCount() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenInfoMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UI_SCREEN_INFO_METRICS_PROVIDER_H_
