// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/display_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/assistant/test_support/expect_utils.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "chromeos/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

using chromeos::assistant::AndroidAppInfo;
using chromeos::assistant::InteractionInfo;

constexpr int kSampleInteractionId = 123;
constexpr char kSampleUserId[] = "user-id";
constexpr char kSamplePackageName[] = "app.test";

class AssistantClientMock : public AssistantClient {
 public:
  AssistantClientMock(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      : AssistantClient(std::move(assistant_manager),
                        assistant_manager_internal) {}
  ~AssistantClientMock() override = default;

  // AssistantClient:
  MOCK_METHOD(bool, StartGrpcServices, ());
  MOCK_METHOD(void,
              AddExperimentIds,
              (const std::vector<std::string>& exp_ids));
  MOCK_METHOD(void,
              SendVoicelessInteraction,
              (const ::assistant::api::Interaction& interaction,
               const std::string& description,
               const ::assistant::api::VoicelessOptions& options,
               base::OnceCallback<void(bool)> on_done));
};

class AssistantManagerInternalMock
    : public assistant::FakeAssistantManagerInternal {
 public:
  AssistantManagerInternalMock() = default;
  AssistantManagerInternalMock(const AssistantManagerInternalMock&) = delete;
  AssistantManagerInternalMock& operator=(const AssistantManagerInternalMock&) =
      delete;
  ~AssistantManagerInternalMock() override = default;

  // assistant::FakeAssistantManagerInternal implementation:
  MOCK_METHOD(void,
              SetDisplayConnection,
              (assistant_client::DisplayConnection * connection));
  MOCK_METHOD(void,
              SendVoicelessInteraction,
              (const std::string& interaction_proto,
               const std::string& description,
               const assistant_client::VoicelessOptions& options,
               assistant_client::SuccessCallbackInternal on_done));
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

  AssistantManagerInternalMock& assistant_manager_internal_mock() {
    return assistant_manager_internal_;
  }

  AssistantClientMock& assistant_client_mock() { return assistant_client_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  mojo::RemoteSet<mojom::SpeechRecognitionObserver>
      speech_recognition_observers_;
  std::unique_ptr<DisplayController> controller_;
  AssistantManagerInternalMock assistant_manager_internal_;
  AssistantClientMock assistant_client_{nullptr, &assistant_manager_internal_};
};

TEST_F(DisplayControllerTest, ShouldSetDisplayConnection) {
  EXPECT_CALL(assistant_manager_internal_mock(), SetDisplayConnection);

  StartLibassistant();
}

TEST_F(DisplayControllerTest,
       ShouldSendVoicelessInteractionOnVerifyAndroidApp) {
  EXPECT_CALL(assistant_manager_internal_mock(), SetDisplayConnection);
  StartLibassistant();

  AndroidAppInfo app_info;
  app_info.package_name = kSamplePackageName;
  InteractionInfo interaction = {kSampleInteractionId, kSampleUserId};

  EXPECT_CALL(assistant_client_mock(), SendVoicelessInteraction);
  controller()->OnVerifyAndroidApp({app_info}, interaction);
}

}  // namespace libassistant
}  // namespace chromeos
