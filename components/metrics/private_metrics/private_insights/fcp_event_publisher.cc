// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_event_publisher.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace private_insights {

namespace {

void LogFcpMethodExecution(absl::string_view name,
                           absl::string_view details = "") {
  if (details.empty()) {
    VLOG(2) << "FCP event: " << name;
  } else {
    VLOG(2) << "FCP event: " << name << ": " << details;
  }
}

void LogFcpEvent(absl::string_view name,
                 FcpEvent event,
                 absl::string_view details = "") {
  base::UmaHistogramEnumeration(kFcpEventHistogram, event);
  LogFcpMethodExecution(name, details);
}

}  // namespace

FcpSecAggEventPublisher::FcpSecAggEventPublisher() = default;
FcpSecAggEventPublisher::~FcpSecAggEventPublisher() = default;

void FcpSecAggEventPublisher::PublishStateTransition(fcp::secagg::ClientState,
                                                     size_t,
                                                     size_t) {}

void FcpSecAggEventPublisher::PublishError() {}

void FcpSecAggEventPublisher::PublishAbort(bool, const std::string&) {}

void FcpSecAggEventPublisher::set_execution_session_id(int64_t) {}

FcpEventPublisher::FcpEventPublisher() = default;
FcpEventPublisher::~FcpEventPublisher() = default;

void FcpEventPublisher::PublishEligibilityEvalCheckin() {
  LogFcpEvent("EligibilityEvalCheckin", FcpEvent::kEligibilityEvalCheckin);
}

void FcpEventPublisher::PublishEligibilityEvalPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalPlanUriReceived",
              FcpEvent::kEligibilityEvalPlanUriReceived);
}

void FcpEventPublisher::PublishEligibilityEvalPlanReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalPlanReceived",
              FcpEvent::kEligibilityEvalPlanReceived);
}

void FcpEventPublisher::PublishEligibilityEvalNotConfigured(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalNotConfigured",
              FcpEvent::kEligibilityEvalNotConfigured);
}

void FcpEventPublisher::PublishEligibilityEvalRejected(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalRejected", FcpEvent::kEligibilityEvalRejected);
}

void FcpEventPublisher::PublishCheckin() {
  LogFcpEvent("Checkin", FcpEvent::kCheckin);
}

void FcpEventPublisher::PublishCheckinFinished(const fcp::client::NetworkStats&,
                                               FcpDuration) {
  LogFcpEvent("CheckinFinished", FcpEvent::kCheckinFinished);
}

void FcpEventPublisher::PublishRejected() {
  LogFcpEvent("Rejected", FcpEvent::kRejected);
}

void FcpEventPublisher::PublishTensorFlowError(
    int,
    absl::string_view error_message) {
  LogFcpEvent("TensorFlowError", FcpEvent::kTensorFlowError, error_message);
}

void FcpEventPublisher::PublishIoError(absl::string_view error_message) {
  LogFcpEvent("IoError", FcpEvent::kIoError, error_message);
}

void FcpEventPublisher::PublishExampleSelectorError(
    int,
    absl::string_view error_message) {
  LogFcpEvent("ExampleSelectorError", FcpEvent::kExampleSelectorError,
              error_message);
}

void FcpEventPublisher::PublishInterruption(const fcp::client::ExampleStats&,
                                            FcpTime) {
  LogFcpEvent("Interruption", FcpEvent::kInterruption);
}

void FcpEventPublisher::PublishTaskNotStarted(absl::string_view error_message) {
  LogFcpEvent("TaskNotStarted", FcpEvent::kTaskNotStarted, error_message);
}

void FcpEventPublisher::PublishNonfatalInitializationError(
    absl::string_view error_message) {
  LogFcpEvent("NonfatalInitializationError",
              FcpEvent::kNonfatalInitializationError, error_message);
}

void FcpEventPublisher::PublishFatalInitializationError(
    absl::string_view error_message) {
  LogFcpEvent("FatalInitializationError", FcpEvent::kFatalInitializationError,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinIoError(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalCheckinIoError",
              FcpEvent::kEligibilityEvalCheckinIoError, error_message);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinClientInterrupted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalCheckinClientInterrupted",
              FcpEvent::kEligibilityEvalCheckinClientInterrupted,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinServerAborted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalCheckinServerAborted",
              FcpEvent::kEligibilityEvalCheckinServerAborted, error_message);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinErrorInvalidPayload(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalCheckinErrorInvalidPayload",
              FcpEvent::kEligibilityEvalCheckinErrorInvalidPayload,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationStarted() {
  LogFcpEvent("EligibilityEvalComputationStarted",
              FcpEvent::kEligibilityEvalComputationStarted);
}

void FcpEventPublisher::PublishEligibilityEvalComputationInvalidArgument(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationInvalidArgument",
              FcpEvent::kEligibilityEvalComputationInvalidArgument,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationIOError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationIOError",
              FcpEvent::kEligibilityEvalComputationIOError, error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationExampleIteratorError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationExampleIteratorError",
              FcpEvent::kEligibilityEvalComputationExampleIteratorError,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationTensorflowError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationTensorflowError",
              FcpEvent::kEligibilityEvalComputationTensorflowError,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationInterrupted(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationInterrupted",
              FcpEvent::kEligibilityEvalComputationInterrupted, error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationErrorNonfatal(
    absl::string_view error_message) {
  LogFcpEvent("EligibilityEvalComputationErrorNonfatal",
              FcpEvent::kEligibilityEvalComputationErrorNonfatal,
              error_message);
}

void FcpEventPublisher::PublishEligibilityEvalComputationCompleted(
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LogFcpEvent("EligibilityEvalComputationCompleted",
              FcpEvent::kEligibilityEvalComputationCompleted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsStarted() {
  LogFcpEvent("MultipleTaskAssignmentsStarted",
              FcpEvent::kMultipleTaskAssignmentsStarted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsIOError(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsIOError",
              FcpEvent::kMultipleTaskAssignmentsIOError, error_message);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPayloadIOError(
    absl::string_view error_message) {
  LogFcpEvent("MultipleTaskAssignmentsPayloadIOError",
              FcpEvent::kMultipleTaskAssignmentsPayloadIOError, error_message);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsInvalidPayload(
    absl::string_view error_message) {
  LogFcpEvent("MultipleTaskAssignmentsInvalidPayload",
              FcpEvent::kMultipleTaskAssignmentsInvalidPayload, error_message);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsClientInterrupted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsClientInterrupted",
              FcpEvent::kMultipleTaskAssignmentsClientInterrupted,
              error_message);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsServerAborted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsServerAborted",
              FcpEvent::kMultipleTaskAssignmentsServerAborted, error_message);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsTurnedAway(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsTurnedAway",
              FcpEvent::kMultipleTaskAssignmentsTurnedAway);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsPlanUriReceived",
              FcpEvent::kMultipleTaskAssignmentsPlanUriReceived);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPlanUriPartialReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsPlanUriPartialReceived",
              FcpEvent::kMultipleTaskAssignmentsPlanUriPartialReceived);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPartialCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsPartialCompleted",
              FcpEvent::kMultipleTaskAssignmentsPartialCompleted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("MultipleTaskAssignmentsCompleted",
              FcpEvent::kMultipleTaskAssignmentsCompleted);
}

void FcpEventPublisher::PublishCheckinIoError(absl::string_view error_message,
                                              const fcp::client::NetworkStats&,
                                              FcpDuration) {
  LogFcpEvent("CheckinIoError", FcpEvent::kCheckinIoError, error_message);
}

void FcpEventPublisher::PublishCheckinClientInterrupted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("CheckinClientInterrupted", FcpEvent::kCheckinClientInterrupted,
              error_message);
}

void FcpEventPublisher::PublishCheckinServerAborted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("CheckinServerAborted", FcpEvent::kCheckinServerAborted,
              error_message);
}

void FcpEventPublisher::PublishCheckinInvalidPayload(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("CheckinInvalidPayload", FcpEvent::kCheckinInvalidPayload,
              error_message);
}

void FcpEventPublisher::PublishRejected(const fcp::client::NetworkStats&,
                                        FcpDuration) {
  LogFcpEvent("Rejected", FcpEvent::kRejected);
}

void FcpEventPublisher::PublishCheckinPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("CheckinPlanUriReceived", FcpEvent::kCheckinPlanUriReceived);
}

void FcpEventPublisher::PublishCheckinFinishedV2(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("CheckinFinishedV2", FcpEvent::kCheckinFinishedV2);
}

void FcpEventPublisher::PublishComputationStarted() {
  LogFcpEvent("ComputationStarted", FcpEvent::kComputationStarted);
}

void FcpEventPublisher::PublishComputationInvalidArgument(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationInvalidArgument",
              FcpEvent::kComputationInvalidArgument, error_message);
}

void FcpEventPublisher::PublishComputationIOError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationIOError", FcpEvent::kComputationIOError,
              error_message);
}

void FcpEventPublisher::PublishComputationExampleIteratorError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationExampleIteratorError",
              FcpEvent::kComputationExampleIteratorError, error_message);
}

void FcpEventPublisher::PublishComputationTensorflowError(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationTensorflowError",
              FcpEvent::kComputationTensorflowError, error_message);
}

void FcpEventPublisher::PublishComputationInterrupted(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationInterrupted", FcpEvent::kComputationInterrupted,
              error_message);
}

void FcpEventPublisher::PublishComputationCompleted(
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationCompleted", FcpEvent::kComputationCompleted);
}

void FcpEventPublisher::PublishComputationInsufficientData(
    absl::string_view error_message,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ComputationInsufficientData",
              FcpEvent::kComputationInsufficientData, error_message);
}

void FcpEventPublisher::PublishResultUploadStarted() {
  LogFcpEvent("ResultUploadStarted", FcpEvent::kResultUploadStarted);
}

void FcpEventPublisher::PublishResultUploadIOError(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ResultUploadIOError", FcpEvent::kResultUploadIOError,
              error_message);
}

void FcpEventPublisher::PublishResultUploadClientInterrupted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ResultUploadClientInterrupted",
              FcpEvent::kResultUploadClientInterrupted, error_message);
}

void FcpEventPublisher::PublishResultUploadServerAborted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ResultUploadServerAborted", FcpEvent::kResultUploadServerAborted,
              error_message);
}

void FcpEventPublisher::PublishResultUploadCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("ResultUploadCompleted", FcpEvent::kResultUploadCompleted);
}

void FcpEventPublisher::PublishFailureUploadStarted() {
  LogFcpEvent("FailureUploadStarted", FcpEvent::kFailureUploadStarted);
}

void FcpEventPublisher::PublishFailureUploadIOError(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("FailureUploadIOError", FcpEvent::kFailureUploadIOError,
              error_message);
}

void FcpEventPublisher::PublishFailureUploadClientInterrupted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("FailureUploadClientInterrupted",
              FcpEvent::kFailureUploadClientInterrupted, error_message);
}

void FcpEventPublisher::PublishFailureUploadServerAborted(
    absl::string_view error_message,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("FailureUploadServerAborted",
              FcpEvent::kFailureUploadServerAborted, error_message);
}

void FcpEventPublisher::PublishFailureUploadCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LogFcpEvent("FailureUploadCompleted", FcpEvent::kFailureUploadCompleted);
}

void FcpEventPublisher::SetModelIdentifier(
    const std::string& model_identifier) {
  LogFcpMethodExecution("SetModelIdentifier", model_identifier);
}

fcp::client::SecAggEventPublisher* FcpEventPublisher::secagg_event_publisher() {
  return &secagg_event_publisher_;
}

}  // namespace private_insights
