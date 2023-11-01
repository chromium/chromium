// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/display_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/assistant/test_support/expect_utils.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/fake_assistant_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

using assistant::AndroidAppInfo;
using chromeos::assistant::InteractionInfo;

constexpr int kSampleInteractionId = 123;
constexpr char kSampleUserId[] = "user-id";
constexpr char kSamplePackageName[] = "app.test";

class AssistantClientMock : public FakeAssistantClient {
 public:
  AssistantClientMock(std::unique_ptr<chromeos::assistant::FakeAssistantManager>
                          assistant_manager)
      : FakeAssistantClient(std::move(assistant_manager)) {}
  ~AssistantClientMock() override = default;

  // AssistantClient:
  MOCK_METHOD(void,
              SendVoicelessInteraction,
              (const ::assistant::api::Interaction& interaction,
               const std::string& description,
               const ::assistant::api::VoicelessOptions& options,
               base::OnceCallback<void(bool)> on_done));

  MOCK_METHOD(void,
              AddDisplayEventObserver,
              (GrpcServicesObserver<OnAssistantDisplayEventRequest> *
               observer));
};

}  // namespace

class DisplayControllerTest : public ::testing::Test {
 public:
  DisplayControllerTest() = default;
  DisplayControllerTest(const DisplayControllerTest&) = delete;
  DisplayControllerTest& operator=(const DisplayControllerTest&) = delete;
  ~DisplayControllerTest() override = default;

  void SetUp() override {
    controller_ =
        std::make_unique<DisplayController>(&speech_recognition_observers_);
  }

  void StartLibassistant() {
    controller_->OnAssistantClientCreated(&assistant_client_);
  }

  DisplayController* controller() { return controller_.get(); }

  AssistantClientMock& assistant_client_mock() { return assistant_client_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  mojo::RemoteSet<mojom::SpeechRecognitionObserver>
      speech_recognition_observers_;
  std::unique_ptr<DisplayController> controller_;
  AssistantClientMock assistant_client_{nullptr};
};

TEST_F(DisplayControllerTest, ShouldSetDisplayEventObserver) {
  EXPECT_CALL(assistant_client_mock(), AddDisplayEventObserver);

  StartLibassistant();
}

TEST_F(DisplayControllerTest,
       ShouldSendVoicelessInteractionOnVerifyAndroidApp) {
  EXPECT_CALL(assistant_client_mock(), AddDisplayEventObserver);
  StartLibassistant();

  AndroidAppInfo app_info;
  app_info.package_name = kSamplePackageName;
  InteractionInfo interaction = {kSampleInteractionId, kSampleUserId};

  EXPECT_CALL(assistant_client_mock(), SendVoicelessInteraction);
  controller()->OnVerifyAndroidApp({app_info}, interaction);
}

}  // namespace ash::libassistant
