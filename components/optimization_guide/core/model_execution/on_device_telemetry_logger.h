// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_

#include <cstdint>
#include <optional>

#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

namespace optimization_guide {

// A helper class to log telemetry and result histograms for a single on-device
// model execution request. This is shared between OnDeviceExecution and custom
// execution paths like Prompt API. This class should be created per
// call/request to a session.
class OnDeviceRequestTelemetryLogger {
 public:
  // Possible outcomes of ExecuteModel().
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(OptimizationGuideOnDeviceExecuteModelResult)
  enum class Result {
    // On-device was not used.
    kOnDeviceNotUsed = 0,
    // On-device was used, and it completed successfully.
    kUsedOnDevice = 1,
    // Failed constructing message, and used server.
    kFailedConstructingMessage = 2,
    // Got a response from on-device, but failed constructing the message.
    kFailedConstructingResponseMessage = 3,
    // Timed out and used server.
    kTimedOut = 4,
    // Received a disconnect while waiting for response. This may trigger
    // fallback to another model, e.g. on the server, if configured.
    kDisconnectAndMaybeFallback = 5,
    // Received a disconnect while waiting for response and cancelled.
    kDisconnectAndCancel = 6,
    // Response was cancelled (e.g. because ExecuteModel() was called while
    // waiting for response or the client cancelled the request).
    kCancelled = 7,
    // SessionImpl was destroyed while waiting for a response.
    kDestroyedWhileWaitingForResponse = 8,
    // On-device was used, it completed successfully, but the output is
    // considered unsafe.
    kUsedOnDeviceOutputUnsafe = 9,
    // On-device was used, but the output was rejected (because contained PII).
    kContainedPII = 10,
    // On-device was used, but the output was rejected because it had repeats.
    kResponseHadRepeats = 11,
    // On-device was used and the output was complete but the output was
    // rejected since it did not have the required safety scores.
    kResponseCompleteButNoRequiredSafetyScores = 12,
    // On-device was used and completed successfully, but the output was not in
    // a language that could be reliably evaluated for safety.
    kUsedOnDeviceOutputUnsupportedLanguage = 13,
    // On-device was used and completed successfully, but failed constructing
    // the text safety remote request.
    kFailedConstructingRemoteTextSafetyRequest = 14,
    // On-device was used and completed successfully, but the text safety remote
    // request failed for some reason.
    kTextSafetyRemoteRequestFailed = 15,
    // On-device was used, but the request was considered unsafe.
    kRequestUnsafe = 16,

    // Please update OptimizationGuideOnDeviceExecuteModelResult in
    // optimization/enums.xml.
    kMaxValue = kRequestUnsafe,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideOnDeviceExecuteModelResult)

  // Creates a logger that only records timing/token telemetry (e.g. for
  // context-only Append() operations).
  explicit OnDeviceRequestTelemetryLogger(mojom::OnDeviceFeature feature);

  // Creates a logger that records both timing/token telemetry and execution
  // result/completion histograms on destruction.
  OnDeviceRequestTelemetryLogger(mojom::OnDeviceFeature feature,
                                 OnDeviceBaseModel base_model);
  OnDeviceRequestTelemetryLogger(const OnDeviceRequestTelemetryLogger&) =
      delete;
  OnDeviceRequestTelemetryLogger& operator=(
      const OnDeviceRequestTelemetryLogger&) = delete;
  ~OnDeviceRequestTelemetryLogger();

  void set_result(Result result) { result_ = result; }

  // Starts the timer for latency telemetry histograms.
  void StartTelemetryTimer();

  // Records the time to the first response chunk.
  void RecordFirstResponse();

  // Records the context processing time (prefill time).
  void RecordContextTime();

  // Records metrics on completion of response generation.
  void RecordCompletion(uint32_t num_tokens);

  // Records the time and result when destroyed while waiting for response.
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

  // Execution result and completion histograms are only logged on destruction
  // when `base_model_` is populated.
  const std::optional<OnDeviceBaseModel> base_model_;
  Result result_ = Result::kOnDeviceNotUsed;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_TELEMETRY_LOGGER_H_
