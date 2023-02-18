// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/speaker_id_enrollment_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
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

  void SetUp() override {
    // TODO(b/269803444): Reenable tests for LibAssistantV2.
    feature_list_.InitAndDisableFeature(
        assistant::features::kEnableLibAssistantV2);
  }

  mojom::SpeakerIdEnrollmentController& controller() {
    return service_tester_.speaker_id_enrollment_controller();
  }

  void FlushForTesting() { service_tester_.FlushForTesting(); }

  LibassistantServiceTester& service_tester() { return service_tester_; }

  chromeos::assistant::FakeAssistantManagerInternal&
  assistant_manager_internal() {
    return service_tester().assistant_manager_internal();
  }

  assistant_client::SpeakerIdEnrollmentUpdate CreateUpdate(
      SpeakerIdEnrollmentState state) {
    assistant_client::SpeakerIdEnrollmentUpdate result;
    result.state = state;
    return result;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  base::test::ScopedFeatureList feature_list_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       StartShouldBeANoopIfLibassistantIsNotStarted_V1) {
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());

  FlushForTesting();

  EXPECT_FALSE(
      assistant_manager_internal().is_speaker_id_enrollment_in_progress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       StopShouldBeANoopIfLibassistantIsNotStarted) {
  controller().StopSpeakerIdEnrollment();

  FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldBeAbleToStartEnrollment_V1) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());

  FlushForTesting();

  EXPECT_TRUE(
      assistant_manager_internal().is_speaker_id_enrollment_in_progress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldBeAbleToStopEnrollment_V1) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());
  controller().StopSpeakerIdEnrollment();

  FlushForTesting();

  EXPECT_FALSE(
      assistant_manager_internal().is_speaker_id_enrollment_in_progress());
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldPassEnrollmentUpdatesToClient_V1) {
  service_tester().Start();
  StrictMock<SpeakerIdEnrollmentClientMock> client;

  controller().StartSpeakerIdEnrollment("user-id",
                                        /*skip_cloud_enrollment=*/false,
                                        client.BindNewPipeAndPassRemote());
  FlushForTesting();
  ASSERT_TRUE(
      assistant_manager_internal().is_speaker_id_enrollment_in_progress());

  auto callback =
      assistant_manager_internal().speaker_id_enrollment_update_callback();
  ASSERT_TRUE(callback);

  // These are ignored
  callback(CreateUpdate(SpeakerIdEnrollmentState::INIT));
  callback(CreateUpdate(SpeakerIdEnrollmentState::CHECK));
  callback(CreateUpdate(SpeakerIdEnrollmentState::UPLOAD));
  callback(CreateUpdate(SpeakerIdEnrollmentState::FETCH));
  client.FlushForTesting();

  EXPECT_CALL(client, OnListeningHotword);
  callback(CreateUpdate(SpeakerIdEnrollmentState::LISTEN));
  client.FlushForTesting();

  EXPECT_CALL(client, OnProcessingHotword);
  callback(CreateUpdate(SpeakerIdEnrollmentState::PROCESS));
  client.FlushForTesting();

  EXPECT_CALL(client, OnSpeakerIdEnrollmentDone);
  callback(CreateUpdate(SpeakerIdEnrollmentState::DONE));
  client.FlushForTesting();

  EXPECT_CALL(client, OnSpeakerIdEnrollmentFailure);
  callback(CreateUpdate(SpeakerIdEnrollmentState::FAILURE));
  client.FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldStartSecondEnrollmentEvenIfFirstIsOngoing_V1) {
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

  // Note: our FakeAssistantManagerInternal ensures the first enrollment was
  // stopped (with an EXPECT_FALSE).
  // To ensure the second enrollment was started, we send an enrollment update,
  // which should be received by the second client and not the first one.

  auto callback =
      assistant_manager_internal().speaker_id_enrollment_update_callback();
  ASSERT_TRUE(callback);

  EXPECT_CALL(second_client, OnProcessingHotword);
  callback(CreateUpdate(SpeakerIdEnrollmentState::PROCESS));
  first_client.FlushForTesting();
  second_client.FlushForTesting();
}

TEST_F(
    AssistantSpeakerIdEnrollmentControllerTest,
    GetSpeakerIdEnrollmentStatusShouldReturnFalseIfLibassistantIsNotStarted_V1) {
  base::MockCallback<GetSpeakerIdEnrollmentStatusCallback> callback;

  EXPECT_CALL(callback, Run).WillOnce([](auto response) {
    EXPECT_FALSE(response->user_model_exists);
  });

  controller().GetSpeakerIdEnrollmentStatus("user-id", callback.Get());

  FlushForTesting();
}

TEST_F(AssistantSpeakerIdEnrollmentControllerTest,
       ShouldReturnSpeakerIdEnrollmentStatus_V1) {
  service_tester().Start();

  base::MockCallback<GetSpeakerIdEnrollmentStatusCallback> callback;
  EXPECT_CALL(callback, Run).WillOnce([](auto response) {
    EXPECT_TRUE(response->user_model_exists);
  });

  controller().GetSpeakerIdEnrollmentStatus("user-id", callback.Get());
  FlushForTesting();

  auto libassistant_callback =
      assistant_manager_internal().speaker_id_enrollment_status_callback();
  libassistant_callback(SpeakerIdEnrollmentStatus{/*user_model_exists=*/true});

  FlushForTesting();
}

}  // namespace ash::libassistant
