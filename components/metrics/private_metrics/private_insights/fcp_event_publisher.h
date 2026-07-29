// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_EVENT_PUBLISHER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_EVENT_PUBLISHER_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "third_party/federated_compute/chromium/fcp/secagg/shared/secagg_messages.pb.h"
#include "third_party/federated_compute/src/fcp/client/event_publisher.h"
#include "third_party/federated_compute/src/fcp/client/secagg_event_publisher.h"
#include "third_party/federated_compute/src/fcp/client/stats.h"

namespace private_insights {

inline constexpr char kFcpEventHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpEvent";

// LINT.IfChange(PrivateInsightsFcpEvent)
enum class FcpEvent {
  kEligibilityEvalCheckin = 0,
  kEligibilityEvalPlanUriReceived = 1,
  kEligibilityEvalPlanReceived = 2,
  kEligibilityEvalNotConfigured = 3,
  kEligibilityEvalRejected = 4,
  kCheckin = 5,
  kCheckinFinished = 6,
  kRejected = 7,
  kTensorFlowError = 8,
  kIoError = 9,
  kExampleSelectorError = 10,
  kInterruption = 11,
  kTaskNotStarted = 12,
  kNonfatalInitializationError = 13,
  kFatalInitializationError = 14,
  kEligibilityEvalCheckinIoError = 15,
  kEligibilityEvalCheckinClientInterrupted = 16,
  kEligibilityEvalCheckinServerAborted = 17,
  kEligibilityEvalCheckinErrorInvalidPayload = 18,
  kEligibilityEvalComputationStarted = 19,
  kEligibilityEvalComputationInvalidArgument = 20,
  kEligibilityEvalComputationIOError = 21,
  kEligibilityEvalComputationExampleIteratorError = 22,
  kEligibilityEvalComputationTensorflowError = 23,
  kEligibilityEvalComputationInterrupted = 24,
  kEligibilityEvalComputationErrorNonfatal = 25,
  kEligibilityEvalComputationCompleted = 26,
  kMultipleTaskAssignmentsStarted = 27,
  kMultipleTaskAssignmentsIOError = 28,
  kMultipleTaskAssignmentsPayloadIOError = 29,
  kMultipleTaskAssignmentsInvalidPayload = 30,
  kMultipleTaskAssignmentsClientInterrupted = 31,
  kMultipleTaskAssignmentsServerAborted = 32,
  kMultipleTaskAssignmentsTurnedAway = 33,
  kMultipleTaskAssignmentsPlanUriReceived = 34,
  kMultipleTaskAssignmentsPlanUriPartialReceived = 35,
  kMultipleTaskAssignmentsPartialCompleted = 36,
  kMultipleTaskAssignmentsCompleted = 37,
  kCheckinIoError = 38,
  kCheckinClientInterrupted = 39,
  kCheckinServerAborted = 40,
  kCheckinInvalidPayload = 41,
  kCheckinPlanUriReceived = 42,
  kCheckinFinishedV2 = 43,
  kComputationStarted = 44,
  kComputationInvalidArgument = 45,
  kComputationIOError = 46,
  kComputationExampleIteratorError = 47,
  kComputationTensorflowError = 48,
  kComputationInterrupted = 49,
  kComputationCompleted = 50,
  kComputationInsufficientData = 51,
  kResultUploadStarted = 52,
  kResultUploadIOError = 53,
  kResultUploadClientInterrupted = 54,
  kResultUploadServerAborted = 55,
  kResultUploadCompleted = 56,
  kFailureUploadStarted = 57,
  kFailureUploadIOError = 58,
  kFailureUploadClientInterrupted = 59,
  kFailureUploadServerAborted = 60,
  kFailureUploadCompleted = 61,
  kMaxValue = kFailureUploadCompleted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PrivateInsightsFcpEvent)

class FcpSecAggEventPublisher : public fcp::client::SecAggEventPublisher {
 public:
  FcpSecAggEventPublisher();
  ~FcpSecAggEventPublisher() override;

  void PublishStateTransition(fcp::secagg::ClientState state,
                              size_t last_sent_message_size,
                              size_t last_received_message_size) override;
  void PublishError() override;
  void PublishAbort(bool client_initiated,
                    const std::string& error_message) override;
  void set_execution_session_id(int64_t execution_session_id) override;
};

class FcpEventPublisher : public fcp::client::EventPublisher {
 private:
  using FcpDuration = absl::Duration;  // nocheck
  using FcpTime = absl::Time;          // nocheck

 public:
  FcpEventPublisher();
  ~FcpEventPublisher() override;

  void PublishEligibilityEvalCheckin() override;
  void PublishEligibilityEvalPlanUriReceived(const fcp::client::NetworkStats&,
                                             FcpDuration) override;
  void PublishEligibilityEvalPlanReceived(const fcp::client::NetworkStats&,
                                          FcpDuration) override;
  void PublishEligibilityEvalNotConfigured(const fcp::client::NetworkStats&,
                                           FcpDuration) override;
  void PublishEligibilityEvalRejected(const fcp::client::NetworkStats&,
                                      FcpDuration) override;
  void PublishCheckin() override;
  void PublishCheckinFinished(const fcp::client::NetworkStats&,
                              FcpDuration) override;
  void PublishRejected() override;
  void PublishTensorFlowError(int, absl::string_view error_message) override;
  void PublishIoError(absl::string_view error_message) override;
  void PublishExampleSelectorError(int,
                                   absl::string_view error_message) override;
  void PublishInterruption(const fcp::client::ExampleStats&, FcpTime) override;
  void PublishTaskNotStarted(absl::string_view error_message) override;
  void PublishNonfatalInitializationError(
      absl::string_view error_message) override;
  void PublishFatalInitializationError(
      absl::string_view error_message) override;
  void PublishEligibilityEvalCheckinIoError(absl::string_view error_message,
                                            const fcp::client::NetworkStats&,
                                            FcpDuration) override;
  void PublishEligibilityEvalCheckinClientInterrupted(
      absl::string_view error_message,
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishEligibilityEvalCheckinServerAborted(
      absl::string_view error_message,
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishEligibilityEvalCheckinErrorInvalidPayload(
      absl::string_view error_message,
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationStarted() override;
  void PublishEligibilityEvalComputationInvalidArgument(
      absl::string_view error_message,
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationIOError(
      absl::string_view error_message,
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationExampleIteratorError(
      absl::string_view error_message,
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationTensorflowError(
      absl::string_view error_message,
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationInterrupted(
      absl::string_view error_message,
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishEligibilityEvalComputationErrorNonfatal(
      absl::string_view error_message) override;
  void PublishEligibilityEvalComputationCompleted(
      const fcp::client::ExampleStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsStarted() override;
  void PublishMultipleTaskAssignmentsIOError(absl::string_view error_message,
                                             const fcp::client::NetworkStats&,
                                             FcpDuration) override;
  void PublishMultipleTaskAssignmentsPayloadIOError(
      absl::string_view error_message) override;
  void PublishMultipleTaskAssignmentsInvalidPayload(
      absl::string_view error_message) override;
  void PublishMultipleTaskAssignmentsClientInterrupted(
      absl::string_view error_message,
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsServerAborted(
      absl::string_view error_message,
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsTurnedAway(
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsPlanUriReceived(
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsPlanUriPartialReceived(
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsPartialCompleted(
      const fcp::client::NetworkStats&,
      FcpDuration) override;
  void PublishMultipleTaskAssignmentsCompleted(const fcp::client::NetworkStats&,
                                               FcpDuration) override;
  void PublishCheckinIoError(absl::string_view error_message,
                             const fcp::client::NetworkStats&,
                             FcpDuration) override;
  void PublishCheckinClientInterrupted(absl::string_view error_message,
                                       const fcp::client::NetworkStats&,
                                       FcpDuration) override;
  void PublishCheckinServerAborted(absl::string_view error_message,
                                   const fcp::client::NetworkStats&,
                                   FcpDuration) override;
  void PublishCheckinInvalidPayload(absl::string_view error_message,
                                    const fcp::client::NetworkStats&,
                                    FcpDuration) override;
  void PublishRejected(const fcp::client::NetworkStats&, FcpDuration) override;
  void PublishCheckinPlanUriReceived(const fcp::client::NetworkStats&,
                                     FcpDuration) override;
  void PublishCheckinFinishedV2(const fcp::client::NetworkStats&,
                                FcpDuration) override;
  void PublishComputationStarted() override;
  void PublishComputationInvalidArgument(absl::string_view error_message,
                                         const fcp::client::ExampleStats&,
                                         const fcp::client::NetworkStats&,
                                         FcpDuration) override;
  void PublishComputationIOError(absl::string_view error_message,
                                 const fcp::client::ExampleStats&,
                                 const fcp::client::NetworkStats&,
                                 FcpDuration) override;
  void PublishComputationExampleIteratorError(absl::string_view error_message,
                                              const fcp::client::ExampleStats&,
                                              const fcp::client::NetworkStats&,
                                              FcpDuration) override;
  void PublishComputationTensorflowError(absl::string_view error_message,
                                         const fcp::client::ExampleStats&,
                                         const fcp::client::NetworkStats&,
                                         FcpDuration) override;
  void PublishComputationInterrupted(absl::string_view error_message,
                                     const fcp::client::ExampleStats&,
                                     const fcp::client::NetworkStats&,
                                     FcpDuration) override;
  void PublishComputationCompleted(const fcp::client::ExampleStats&,
                                   const fcp::client::NetworkStats&,
                                   FcpDuration) override;
  void PublishComputationInsufficientData(absl::string_view error_message,
                                          const fcp::client::ExampleStats&,
                                          const fcp::client::NetworkStats&,
                                          FcpDuration) override;
  void PublishResultUploadStarted() override;
  void PublishResultUploadIOError(absl::string_view error_message,
                                  const fcp::client::NetworkStats&,
                                  FcpDuration) override;
  void PublishResultUploadClientInterrupted(absl::string_view error_message,
                                            const fcp::client::NetworkStats&,
                                            FcpDuration) override;
  void PublishResultUploadServerAborted(absl::string_view error_message,
                                        const fcp::client::NetworkStats&,
                                        FcpDuration) override;
  void PublishResultUploadCompleted(const fcp::client::NetworkStats&,
                                    FcpDuration) override;
  void PublishFailureUploadStarted() override;
  void PublishFailureUploadIOError(absl::string_view error_message,
                                   const fcp::client::NetworkStats&,
                                   FcpDuration) override;
  void PublishFailureUploadClientInterrupted(absl::string_view error_message,
                                             const fcp::client::NetworkStats&,
                                             FcpDuration) override;
  void PublishFailureUploadServerAborted(absl::string_view error_message,
                                         const fcp::client::NetworkStats&,
                                         FcpDuration) override;
  void PublishFailureUploadCompleted(const fcp::client::NetworkStats&,
                                     FcpDuration) override;

  void SetModelIdentifier(const std::string&) override;

  fcp::client::SecAggEventPublisher* secagg_event_publisher() override;

 private:
  FcpSecAggEventPublisher secagg_event_publisher_;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_EVENT_PUBLISHER_H_
