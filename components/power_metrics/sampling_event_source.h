// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_SAMPLING_EVENT_SOURCE_H_
#define COMPONENTS_POWER_METRICS_SAMPLING_EVENT_SOURCE_H_

#include "base/callback_forward.h"

namespace power_metrics {

// Invokes a callback when a Sample should be requested from all Samplers.
class SamplingEventSource {
 public:
  using SamplingEventCallback = base::RepeatingClosure;

  virtual ~SamplingEventSource() = 0;

  // Starts generating sampling events. Returns whether the operation succeeded.
  // |callback| is invoked for every sampling event.
  virtual bool Start(SamplingEventCallback callback) = 0;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_SAMPLING_EVENT_SOURCE_H_
