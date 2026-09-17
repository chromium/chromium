// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_event_publisher.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace private_insights {

namespace {

struct EventTestCase {
  void (*publish)(FcpEventPublisher&);
  FcpEvent expected_event;
  const char* expected_user_action;
};

}  // namespace

TEST(FcpEventPublisherTest, EmitsHistogramAndUserAction) {
  const EventTestCase kTestCases[] = {
      {[](FcpEventPublisher& p) { p.PublishEligibilityEvalCheckin(); },
       FcpEvent::kEligibilityEvalCheckin,
       "PrivateInsights.FcpEvent.EligibilityEvalCheckin"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalPlanUriReceived({}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalPlanUriReceived,
       "PrivateInsights.FcpEvent.EligibilityEvalPlanUriReceived"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalPlanReceived({}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalPlanReceived,
       "PrivateInsights.FcpEvent.EligibilityEvalPlanReceived"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalNotConfigured({}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalNotConfigured,
       "PrivateInsights.FcpEvent.EligibilityEvalNotConfigured"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalRejected({}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalRejected,
       "PrivateInsights.FcpEvent.EligibilityEvalRejected"},
      {[](FcpEventPublisher& p) { p.PublishCheckin(); }, FcpEvent::kCheckin,
       "PrivateInsights.FcpEvent.Checkin"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinFinished({}, absl::ZeroDuration());
       },
       FcpEvent::kCheckinFinished, "PrivateInsights.FcpEvent.CheckinFinished"},
      {[](FcpEventPublisher& p) { p.PublishRejected(); }, FcpEvent::kRejected,
       "PrivateInsights.FcpEvent.Rejected"},
      {[](FcpEventPublisher& p) { p.PublishTensorFlowError(1, "tf error"); },
       FcpEvent::kTensorFlowError, "PrivateInsights.FcpEvent.TensorFlowError"},
      {[](FcpEventPublisher& p) { p.PublishIoError("io error"); },
       FcpEvent::kIoError, "PrivateInsights.FcpEvent.IoError"},
      {[](FcpEventPublisher& p) {
         p.PublishExampleSelectorError(1, "example selector error");
       },
       FcpEvent::kExampleSelectorError,
       "PrivateInsights.FcpEvent.ExampleSelectorError"},
      {[](FcpEventPublisher& p) {
         p.PublishInterruption({}, absl::UnixEpoch());
       },
       FcpEvent::kInterruption, "PrivateInsights.FcpEvent.Interruption"},
      {[](FcpEventPublisher& p) { p.PublishTaskNotStarted("not started"); },
       FcpEvent::kTaskNotStarted, "PrivateInsights.FcpEvent.TaskNotStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishNonfatalInitializationError("nonfatal init");
       },
       FcpEvent::kNonfatalInitializationError,
       "PrivateInsights.FcpEvent.NonfatalInitializationError"},
      {[](FcpEventPublisher& p) {
         p.PublishFatalInitializationError("fatal init");
       },
       FcpEvent::kFatalInitializationError,
       "PrivateInsights.FcpEvent.FatalInitializationError"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalCheckinIoError("io error", {},
                                                absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalCheckinIoError,
       "PrivateInsights.FcpEvent.EligibilityEvalCheckinIoError"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalCheckinClientInterrupted("interrupted", {},
                                                          absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalCheckinClientInterrupted,
       "PrivateInsights.FcpEvent.EligibilityEvalCheckinClientInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalCheckinServerAborted("aborted", {},
                                                      absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalCheckinServerAborted,
       "PrivateInsights.FcpEvent.EligibilityEvalCheckinServerAborted"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalCheckinErrorInvalidPayload(
             "invalid payload", {}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalCheckinErrorInvalidPayload,
       "PrivateInsights.FcpEvent.EligibilityEvalCheckinErrorInvalidPayload"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationStarted();
       },
       FcpEvent::kEligibilityEvalComputationStarted,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationInvalidArgument(
             "invalid arg", {}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationInvalidArgument,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationInvalidArgument"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationIOError("io error", {},
                                                    absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationIOError,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationExampleIteratorError(
             "iterator error", {}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationExampleIteratorError,
       "PrivateInsights.FcpEvent."
       "EligibilityEvalComputationExampleIteratorError"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationTensorflowError(
             "tf error", {}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationTensorflowError,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationTensorflowError"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationInterrupted("interrupted", {},
                                                        absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationInterrupted,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationErrorNonfatal("nonfatal error");
       },
       FcpEvent::kEligibilityEvalComputationErrorNonfatal,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationErrorNonfatal"},
      {[](FcpEventPublisher& p) {
         p.PublishEligibilityEvalComputationCompleted({}, absl::ZeroDuration());
       },
       FcpEvent::kEligibilityEvalComputationCompleted,
       "PrivateInsights.FcpEvent.EligibilityEvalComputationCompleted"},
      {[](FcpEventPublisher& p) { p.PublishMultipleTaskAssignmentsStarted(); },
       FcpEvent::kMultipleTaskAssignmentsStarted,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsIOError("io error", {},
                                                 absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsIOError,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsPayloadIOError("payload io error");
       },
       FcpEvent::kMultipleTaskAssignmentsPayloadIOError,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPayloadIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsInvalidPayload("invalid payload");
       },
       FcpEvent::kMultipleTaskAssignmentsInvalidPayload,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsInvalidPayload"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsClientInterrupted(
             "client interrupted", {}, absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsClientInterrupted,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsClientInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsServerAborted("server aborted", {},
                                                       absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsServerAborted,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsServerAborted"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsTurnedAway({}, absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsTurnedAway,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsTurnedAway"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsPlanUriReceived({},
                                                         absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsPlanUriReceived,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPlanUriReceived"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsPlanUriPartialReceived(
             {}, absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsPlanUriPartialReceived,
       "PrivateInsights.FcpEvent."
       "MultipleTaskAssignmentsPlanUriPartialReceived"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsPartialCompleted({},
                                                          absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsPartialCompleted,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsPartialCompleted"},
      {[](FcpEventPublisher& p) {
         p.PublishMultipleTaskAssignmentsCompleted({}, absl::ZeroDuration());
       },
       FcpEvent::kMultipleTaskAssignmentsCompleted,
       "PrivateInsights.FcpEvent.MultipleTaskAssignmentsCompleted"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinIoError("io error", {}, absl::ZeroDuration());
       },
       FcpEvent::kCheckinIoError, "PrivateInsights.FcpEvent.CheckinIoError"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinClientInterrupted("client interrupted", {},
                                           absl::ZeroDuration());
       },
       FcpEvent::kCheckinClientInterrupted,
       "PrivateInsights.FcpEvent.CheckinClientInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinServerAborted("server aborted", {},
                                       absl::ZeroDuration());
       },
       FcpEvent::kCheckinServerAborted,
       "PrivateInsights.FcpEvent.CheckinServerAborted"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinInvalidPayload("invalid payload", {},
                                        absl::ZeroDuration());
       },
       FcpEvent::kCheckinInvalidPayload,
       "PrivateInsights.FcpEvent.CheckinInvalidPayload"},
      {[](FcpEventPublisher& p) {
         p.PublishRejected({}, absl::ZeroDuration());
       },
       FcpEvent::kRejected, "PrivateInsights.FcpEvent.Rejected"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinPlanUriReceived({}, absl::ZeroDuration());
       },
       FcpEvent::kCheckinPlanUriReceived,
       "PrivateInsights.FcpEvent.CheckinPlanUriReceived"},
      {[](FcpEventPublisher& p) {
         p.PublishCheckinFinishedV2({}, absl::ZeroDuration());
       },
       FcpEvent::kCheckinFinishedV2,
       "PrivateInsights.FcpEvent.CheckinFinishedV2"},
      {[](FcpEventPublisher& p) { p.PublishComputationStarted(); },
       FcpEvent::kComputationStarted,
       "PrivateInsights.FcpEvent.ComputationStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationInvalidArgument("invalid arg", {}, {},
                                             absl::ZeroDuration());
       },
       FcpEvent::kComputationInvalidArgument,
       "PrivateInsights.FcpEvent.ComputationInvalidArgument"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationIOError("io error", {}, {}, absl::ZeroDuration());
       },
       FcpEvent::kComputationIOError,
       "PrivateInsights.FcpEvent.ComputationIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationExampleIteratorError("iterator error", {}, {},
                                                  absl::ZeroDuration());
       },
       FcpEvent::kComputationExampleIteratorError,
       "PrivateInsights.FcpEvent.ComputationExampleIteratorError"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationTensorflowError("tf error", {}, {},
                                             absl::ZeroDuration());
       },
       FcpEvent::kComputationTensorflowError,
       "PrivateInsights.FcpEvent.ComputationTensorflowError"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationInterrupted("interrupted", {}, {},
                                         absl::ZeroDuration());
       },
       FcpEvent::kComputationInterrupted,
       "PrivateInsights.FcpEvent.ComputationInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationCompleted({}, {}, absl::ZeroDuration());
       },
       FcpEvent::kComputationCompleted,
       "PrivateInsights.FcpEvent.ComputationCompleted"},
      {[](FcpEventPublisher& p) {
         p.PublishComputationInsufficientData("insufficient data", {}, {},
                                              absl::ZeroDuration());
       },
       FcpEvent::kComputationInsufficientData,
       "PrivateInsights.FcpEvent.ComputationInsufficientData"},
      {[](FcpEventPublisher& p) { p.PublishResultUploadStarted(); },
       FcpEvent::kResultUploadStarted,
       "PrivateInsights.FcpEvent.ResultUploadStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishResultUploadIOError("io error", {}, absl::ZeroDuration());
       },
       FcpEvent::kResultUploadIOError,
       "PrivateInsights.FcpEvent.ResultUploadIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishResultUploadClientInterrupted("client interrupted", {},
                                                absl::ZeroDuration());
       },
       FcpEvent::kResultUploadClientInterrupted,
       "PrivateInsights.FcpEvent.ResultUploadClientInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishResultUploadServerAborted("server aborted", {},
                                            absl::ZeroDuration());
       },
       FcpEvent::kResultUploadServerAborted,
       "PrivateInsights.FcpEvent.ResultUploadServerAborted"},
      {[](FcpEventPublisher& p) {
         p.PublishResultUploadCompleted({}, absl::ZeroDuration());
       },
       FcpEvent::kResultUploadCompleted,
       "PrivateInsights.FcpEvent.ResultUploadCompleted"},
      {[](FcpEventPublisher& p) { p.PublishFailureUploadStarted(); },
       FcpEvent::kFailureUploadStarted,
       "PrivateInsights.FcpEvent.FailureUploadStarted"},
      {[](FcpEventPublisher& p) {
         p.PublishFailureUploadIOError("io error", {}, absl::ZeroDuration());
       },
       FcpEvent::kFailureUploadIOError,
       "PrivateInsights.FcpEvent.FailureUploadIOError"},
      {[](FcpEventPublisher& p) {
         p.PublishFailureUploadClientInterrupted("client interrupted", {},
                                                 absl::ZeroDuration());
       },
       FcpEvent::kFailureUploadClientInterrupted,
       "PrivateInsights.FcpEvent.FailureUploadClientInterrupted"},
      {[](FcpEventPublisher& p) {
         p.PublishFailureUploadServerAborted("server aborted", {},
                                             absl::ZeroDuration());
       },
       FcpEvent::kFailureUploadServerAborted,
       "PrivateInsights.FcpEvent.FailureUploadServerAborted"},
      {[](FcpEventPublisher& p) {
         p.PublishFailureUploadCompleted({}, absl::ZeroDuration());
       },
       FcpEvent::kFailureUploadCompleted,
       "PrivateInsights.FcpEvent.FailureUploadCompleted"},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    FcpEventPublisher publisher;

    test_case.publish(publisher);

    histogram_tester.ExpectBucketCount(kFcpEventHistogram,
                                       test_case.expected_event, 1);
    EXPECT_EQ(user_action_tester.GetActionCount(test_case.expected_user_action),
              1);
  }
}

}  // namespace private_insights
