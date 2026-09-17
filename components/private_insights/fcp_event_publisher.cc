// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_event_publisher.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace private_insights {

namespace {

void RecordFcpUserAction(FcpEvent event) {
  switch (event) {
    case FcpEvent::kEligibilityEvalCheckin:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalCheckin"));
      break;
    case FcpEvent::kEligibilityEvalPlanUriReceived:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalPlanUriReceived"));
      break;
    case FcpEvent::kEligibilityEvalPlanReceived:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalPlanReceived"));
      break;
    case FcpEvent::kEligibilityEvalNotConfigured:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalNotConfigured"));
      break;
    case FcpEvent::kEligibilityEvalRejected:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalRejected"));
      break;
    case FcpEvent::kCheckin:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.Checkin"));
      break;
    case FcpEvent::kCheckinFinished:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.CheckinFinished"));
      break;
    case FcpEvent::kRejected:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.Rejected"));
      break;
    case FcpEvent::kTensorFlowError:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.TensorFlowError"));
      break;
    case FcpEvent::kIoError:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.IoError"));
      break;
    case FcpEvent::kExampleSelectorError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ExampleSelectorError"));
      break;
    case FcpEvent::kInterruption:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.Interruption"));
      break;
    case FcpEvent::kTaskNotStarted:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.TaskNotStarted"));
      break;
    case FcpEvent::kNonfatalInitializationError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.NonfatalInitializationError"));
      break;
    case FcpEvent::kFatalInitializationError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FatalInitializationError"));
      break;
    case FcpEvent::kEligibilityEvalCheckinIoError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalCheckinIoError"));
      break;
    case FcpEvent::kEligibilityEvalCheckinClientInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalCheckinClientInterrupted"));
      break;
    case FcpEvent::kEligibilityEvalCheckinServerAborted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalCheckinServerAborted"));
      break;
    case FcpEvent::kEligibilityEvalCheckinErrorInvalidPayload:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent."
                                  "EligibilityEvalCheckinErrorInvalidPayload"));
      break;
    case FcpEvent::kEligibilityEvalComputationStarted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalComputationStarted"));
      break;
    case FcpEvent::kEligibilityEvalComputationInvalidArgument:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent."
                                  "EligibilityEvalComputationInvalidArgument"));
      break;
    case FcpEvent::kEligibilityEvalComputationIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalComputationIOError"));
      break;
    case FcpEvent::kEligibilityEvalComputationExampleIteratorError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent."
          "EligibilityEvalComputationExampleIteratorError"));
      break;
    case FcpEvent::kEligibilityEvalComputationTensorflowError:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent."
                                  "EligibilityEvalComputationTensorflowError"));
      break;
    case FcpEvent::kEligibilityEvalComputationInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalComputationInterrupted"));
      break;
    case FcpEvent::kEligibilityEvalComputationErrorNonfatal:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalComputationErrorNonfatal"));
      break;
    case FcpEvent::kEligibilityEvalComputationCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.EligibilityEvalComputationCompleted"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsStarted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsStarted"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsIOError"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsPayloadIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPayloadIOError"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsInvalidPayload:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsInvalidPayload"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsClientInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsClientInterrupted"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsServerAborted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsServerAborted"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsTurnedAway:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsTurnedAway"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsPlanUriReceived:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPlanUriReceived"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsPlanUriPartialReceived:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent."
          "MultipleTaskAssignmentsPlanUriPartialReceived"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsPartialCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPartialCompleted"));
      break;
    case FcpEvent::kMultipleTaskAssignmentsCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.MultipleTaskAssignmentsCompleted"));
      break;
    case FcpEvent::kCheckinIoError:
      base::RecordAction(
          base::UserMetricsAction("PrivateInsights.FcpEvent.CheckinIoError"));
      break;
    case FcpEvent::kCheckinClientInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.CheckinClientInterrupted"));
      break;
    case FcpEvent::kCheckinServerAborted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.CheckinServerAborted"));
      break;
    case FcpEvent::kCheckinInvalidPayload:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.CheckinInvalidPayload"));
      break;
    case FcpEvent::kCheckinPlanUriReceived:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.CheckinPlanUriReceived"));
      break;
    case FcpEvent::kCheckinFinishedV2:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.CheckinFinishedV2"));
      break;
    case FcpEvent::kComputationStarted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationStarted"));
      break;
    case FcpEvent::kComputationInvalidArgument:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationInvalidArgument"));
      break;
    case FcpEvent::kComputationIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationIOError"));
      break;
    case FcpEvent::kComputationExampleIteratorError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationExampleIteratorError"));
      break;
    case FcpEvent::kComputationTensorflowError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationTensorflowError"));
      break;
    case FcpEvent::kComputationInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationInterrupted"));
      break;
    case FcpEvent::kComputationCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationCompleted"));
      break;
    case FcpEvent::kComputationInsufficientData:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ComputationInsufficientData"));
      break;
    case FcpEvent::kResultUploadStarted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ResultUploadStarted"));
      break;
    case FcpEvent::kResultUploadIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ResultUploadIOError"));
      break;
    case FcpEvent::kResultUploadClientInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ResultUploadClientInterrupted"));
      break;
    case FcpEvent::kResultUploadServerAborted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ResultUploadServerAborted"));
      break;
    case FcpEvent::kResultUploadCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.ResultUploadCompleted"));
      break;
    case FcpEvent::kFailureUploadStarted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FailureUploadStarted"));
      break;
    case FcpEvent::kFailureUploadIOError:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FailureUploadIOError"));
      break;
    case FcpEvent::kFailureUploadClientInterrupted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FailureUploadClientInterrupted"));
      break;
    case FcpEvent::kFailureUploadServerAborted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FailureUploadServerAborted"));
      break;
    case FcpEvent::kFailureUploadCompleted:
      base::RecordAction(base::UserMetricsAction(
          "PrivateInsights.FcpEvent.FailureUploadCompleted"));
      break;
  }
}

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
  RecordFcpUserAction(event);
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
