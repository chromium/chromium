// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/speaker_id_enrollment_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {
using assistant_client::SpeakerIdEnrollmentStatus;
using SpeakerIdEnrollmentState =
    ::assistant_client::SpeakerIdEnrollmentUpdate::State;
using GetSpeakerIdEnrollmentStatusCallback =
    SpeakerIdEnrollmentController::GetSpeakerIdEnrollmentStatusCallback;
using ::testing::NiceMock;
using ::testing::StrictMock;

class SpeakerIdEnrollmentClientMock : public mojom::SpeakerIdEnrollmentClient {
 public:
  SpeakerIdEnrollmentClientMock() = default;
  SpeakerIdEnrollmentClientMock(const SpeakerIdEnrollmentClientMock&) = delete;
  SpeakerIdEnrollmentClientMock& operator=(
      const SpeakerIdEnrollmentClientMock&) = delete;
  ~SpeakerIdEnrollmentClientMock() override = default;

  ::mojo::PendingRemote<SpeakerIdEnrollmentClient> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::SpeakerIdEnrollmentClient implementation:
  MOCK_METHOD(void, OnListeningHotword, ());
  MOCK_METHOD(void, OnProcessingHotword, ());
  MOCK_METHOD(void, OnSpeakerIdEnrollmentDone, ());
  MOCK_METHOD(void, OnSpeakerIdEnrollmentFailure, ());

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  ::mojo::Receiver<mojom::SpeakerIdEnrollmentClient> receiver_{this};
};
}  // namespace

class AssistantSpeakerIdEnrollmentControllerTest : public ::testing::Test {
 public:
  AssistantSpeakerIdEnrollmentControllerTest() = default;
  AssistantSpeakerIdEnrollmentControllerTest(
      const AssistantSpeakerIdEnrollmentControllerTest&) = delete;
  AssistantSpeakerIdEnrollmentControllerTest& operator=(
      const AssistantSpeakerIdEnrollmentControllerTest&) = delete;
  ~AssistantSpeakerIdEnrollmentControllerTest() override = default;

  mojom::SpeakerIdEnrollmentController& controller() {
    return service_tester_.speaker_id_enrollment_controller();
  }

  void FlushForTesting() { service_tester_.FlushForTesting(); }

  LibassistantServiceTester& service_tester() { return service_tester_; }

  assistant_client::SpeakerIdEnrollmentUpdate CreateUpdate(
      SpeakerIdEnrollmentState state) {
    assistant_client::SpeakerIdEnrollmentUpdate result;
    result.state = state;
    return result;
  }

  bool IsSpeakerIdEnrollmentInProgress() {
    return service_tester_.service()
        .speaker_id_enrollment_controller_for_testing()
        .IsSpeakerIdEnrollmentInProgressForTesting();
  }

  void CallUpdateCallback(
      const ::assistant_client::SpeakerIdEnrollmentUpdate& update) {
    ::assistant::api::OnSpeakerIdEnrollmentEventRequest request;
    ::assistant::api::events::SpeakerIdEnrollmentEvent* event =
        request.mutable_event();
    switch (update.state) {
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::INIT: {
        event->mutable_init_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::CHECK: {
        event->mutable_check_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::LISTEN: {
        event->mutable_listen_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::PROCESS: {
        event->mutable_process_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::UPLOAD: {
        event->mutable_upload_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::FETCH: {
        event->mutable_fetch_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::DONE: {
        event->mutable_done_state();
        break;
      }
      case ::assistant_client::SpeakerIdEnrollmentUpdate::State::FAILURE: {
        event->mutable_failure_state();
        break;
      }
    }

    service_tester_.service()
        .speaker_id_enrollment_controller_for_testing()
        .OnGrpcMessageForTesting(std::move(request));
  }

  void CallGetStatusCallback(bool user_model_exists) {
    service_tester_.service()
        .speaker_id_enrollment_controller_for_testing()
        .SendGetStatusResponseForTesting(user_model_exists);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       StartShouldBeANoopIfLibassistantIsNotStarted) {
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());

  FlushForTesting();

  EXPECT_FALSE(IsSpeakerIdEnrollmentInProgress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       StopShouldBeANoopIfLibassistantIsNotStarted) {
  controller().StopSpeakerIdEnrollment();

  FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldBeAbleToStartEnrollment) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());

  FlushForTesting();

  EXPECT_TRUE(IsSpeakerIdEnrollmentInProgress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldBeAbleToStopEnrollment) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());
  controller().StopSpeakerIdEnrollment();

  FlushForTesting();

  EXPECT_FALSE(IsSpeakerIdEnrollmentInProgress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldPassEnrollmentUpdatesToClient) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());
  FlushForTesting();
  EXPECT_TRUE(IsSpeakerIdEnrollmentInProgress());

  // These are ignored
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::INIT));
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::CHECK));
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::UPLOAD));
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::FETCH));
  client.FlushForTesting();

  EXPECT_CALL(client, OnListeningHotword);
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::LISTEN));
  client.FlushForTesting();

  EXPECT_CALL(client, OnProcessingHotword);
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::PROCESS));
  client.FlushForTesting();

  EXPECT_CALL(client, OnSpeakerIdEnrollmentDone);
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::DONE));
  client.FlushForTesting();

  EXPECT_CALL(client, OnSpeakerIdEnrollmentFailure);
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::FAILURE));
  client.FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldStartSecondEnrollmentEvenIfFirstIsOngoing) {
  service_tester().Start();

  StrictMock<SpeakerIdEnrollmentClientMock> first_client;
  StrictMock<SpeakerIdEnrollmentClientMock> second_client;

  controller().StartSpeakerIdEnrollment(
      "first user",
      /*skip_cloud_enrollment=*/false, first_client.BindNewPipeAndPassRemote());

  controller().StartSpeakerIdEnrollment(
      "second user",
      /*skip_cloud_enrollment=*/false,
      second_client.BindNewPipeAndPassRemote());

  FlushForTesting();

  EXPECT_CALL(second_client, OnProcessingHotword);
  CallUpdateCallback(CreateUpdate(SpeakerIdEnrollmentState::PROCESS));

  first_client.FlushForTesting();
  second_client.FlushForTesting();
}

TEST_F(
    AssistantSpeakerIdEnrollmentControllerTest,
    GetSpeakerIdEnrollmentStatusShouldReturnFalseIfLibassistantIsNotStarted) {
  base::MockCallback<GetSpeakerIdEnrollmentStatusCallback> callback;

  EXPECT_CALL(callback, Run).WillOnce([](auto response) {
    EXPECT_FALSE(response->user_model_exists);
  });

  controller().GetSpeakerIdEnrollmentStatus("user-id", callback.Get());

  FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldReturnSpeakerIdEnrollmentStatus) {
  service_tester().Start();

  base::MockCallback<GetSpeakerIdEnrollmentStatusCallback> callback;
  EXPECT_CALL(callback, Run).WillOnce([](auto response) {
    EXPECT_TRUE(response->user_model_exists);
  });

  controller().GetSpeakerIdEnrollmentStatus("user-id", callback.Get());
  FlushForTesting();

  CallGetStatusCallback(/*user_model_exists=*/true);
  FlushForTesting();
}

}  // namespace ash::libassistant
