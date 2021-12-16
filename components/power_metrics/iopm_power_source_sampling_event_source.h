// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_IOPM_POWER_SOURCE_SAMPLING_EVENT_SOURCE_H_
#define COMPONENTS_POWER_METRICS_IOPM_POWER_SOURCE_SAMPLING_EVENT_SOURCE_H_

#include "base/callback.h"
#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "components/power_metrics/sampling_event_source.h"

namespace power_metrics {

// Generates a sampling event when a state change notification is dispatched by
// the IOPMPowerSource service.
class IOPMPowerSourceSamplingEventSource : public SamplingEventSource {
 public:
  IOPMPowerSourceSamplingEventSource();

  ~IOPMPowerSourceSamplingEventSource() override;

  // SamplingEventSource:
  bool Start(SamplingEventCallback callback) override;

 private:
  static void OnNotification(void* context,
                             io_service_t service,
                             natural_t message_type,
                             void* message_argument);

  base::mac::ScopedIONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_service_t> service_;
  base::mac::ScopedIOObject<io_object_t> notification_;
  SamplingEventCallback callback_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_IOPM_POWER_SOURCE_SAMPLING_EVENT_SOURCE_H_
