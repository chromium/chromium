// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/assistant/media_host.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/assistant/test_support/mock_media_manager.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {

using media_session::mojom::MediaSession;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;

class FakeLibassistantV1Api : public LibassistantV1Api {
 public:
  explicit FakeLibassistantV1Api(FakeAssistantManager* assistant_manager)
      : LibassistantV1Api(assistant_manager,
                          &assistant_manager->assistant_manager_internal()) {}
};

}  // namespace

class AssistantMediaSessionTest : public testing::Test {
 public:
  AssistantMediaSessionTest()
      : media_host_(AssistantClient::Get(),
                    /*interaction_subscribers=*/nullptr),
        assistant_media_session_(&media_host_) {}
  ~AssistantMediaSessionTest() override = default;

  AssistantMediaSession* assistant_media_session() {
    return &assistant_media_session_;
  }

  MediaHost& media_host() { return media_host_; }

  void StartMediaHost(MockMediaManager& media_manager_mock) {
    assistant_manager_.SetMediaManager(&media_manager_mock);
    EXPECT_CALL(media_manager_mock, AddListener);
    media_host().Start(&assistant_manager_.assistant_manager_internal());
  }

 private:
  // Needed for a test environment to support post tasks and should outlive
  // other class members.
  base::test::SingleThreadTaskEnvironment task_environment_;

  ScopedAssistantClient client;
  MediaHost media_host_;
  FakeAssistantManager assistant_manager_;
  FakeLibassistantV1Api libassistant_v1_api_{&assistant_manager_};
  AssistantMediaSession assistant_media_session_;
};

TEST_F(AssistantMediaSessionTest, ShouldUpdateSessionStateOnStartStopDucking) {
  assistant_media_session()->StartDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateDucking());

  assistant_media_session()->StopDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());
}

TEST_F(AssistantMediaSessionTest,
       ShouldUpdateSessionStateAndSendActionOnSuspendResumePlaying) {
  testing::StrictMock<MockMediaManager> media_manager;
  StartMediaHost(media_manager);

  // Suspend.
  EXPECT_CALL(media_manager, Pause);
  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());

  // Then resume.
  EXPECT_CALL(media_manager, Resume);
  assistant_media_session()->Resume(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());

  // And pause again.
  EXPECT_CALL(media_manager, Pause);
  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());
}

}  // namespace assistant
}  // namespace chromeos
