// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SAMPLING_METRICS_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SAMPLING_METRICS_PROVIDER_H_

#include "base/timer/timer.h"

namespace web_app {

// This class computes PWA metrics using a sampling approach. The goal is to be
// simple, accurate, and efficient. This class has a timer that triggers every 5
// minutes. When the timer triggers, this class iterates through every browser
// window and computes PWA-related state.
class SamplingMetricsProvider {
 public:
  SamplingMetricsProvider();
  ~SamplingMetricsProvider();

  // Public and static for testing.
  static void EmitMetrics();

 private:
  base::RepeatingTimer timer_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SAMPLING_METRICS_PROVIDER_H_
