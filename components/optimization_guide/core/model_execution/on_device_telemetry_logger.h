// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_

#include "base/time/time.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

namespace optimization_guide {

// A helper class to log telemetry for a single on-device model execution
// request. This is shared between OnDeviceExecution and custom execution paths
// like Prompt API. This class should be created per call/request to a session.
class OnDeviceRequestTelemetryLogger {
 public:
  explicit OnDeviceRequestTelemetryLogger(mojom::OnDeviceFeature feature);
  ~OnDeviceRequestTelemetryLogger();

  // Records the time to the first response chunk.
  void RecordFirstResponse();

  // Records the context processing time (prefill time).
  void RecordContextTime();

  // Records metrics on completion of response generation.
  void RecordCompletion(uint32_t num_tokens);

  // Records the time when destroyed while waiting for response.
  void RecordDestroyedWhileWaiting();

  // Getters for durations (valid after respective Record calls).
  base::TimeDelta GetTimeToFirstResponse() const;
  base::TimeDelta GetTimeToContextProcessing() const;
  base::TimeDelta GetTimeToCompletion() const;
  base::TimeTicks GetStartTime() const;

 private:
  const mojom::OnDeviceFeature feature_;
  base::TimeTicks start_time_;
  base::TimeTicks first_response_time_;
  base::TimeTicks context_time_;
  base::TimeTicks completion_time_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_
