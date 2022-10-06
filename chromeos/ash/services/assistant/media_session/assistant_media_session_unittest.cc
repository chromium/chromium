// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/media_session/assistant_media_session.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/media_host.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/test_support/libassistant_media_controller_mock.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

using media_session::mojom::MediaSession;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;

}  // namespace

class AssistantMediaSessionTest : public testing::Test {
 public:
  AssistantMediaSessionTest() = default;
  ~AssistantMediaSessionTest() override = default;

  void SetUp() override {
    media_host().Initialize(
        &libassistant_media_controller_,
        libassistant_media_delegate_.BindNewPipeAndPassReceiver());
  }

  AssistantMediaSession* assistant_media_session() {
    return &assistant_media_session_;
  }

  MediaHost& media_host() { return media_host_; }

  LibassistantMediaControllerMock& libassistant_media_controller_mock() {
    return libassistant_media_controller_;
  }

 private:
  // Needed for a test environment to support post tasks and should outlive
  // other class members.
  base::test::SingleThreadTaskEnvironment task_environment_;

  ScopedAssistantBrowserDelegate delegate_;
  testing::StrictMock<LibassistantMediaControllerMock>
      libassistant_media_controller_;
  mojo::Remote<libassistant::mojom::MediaDelegate> libassistant_media_delegate_;
  MediaHost media_host_{AssistantBrowserDelegate::Get(),
                        /*interaction_subscribers=*/nullptr};
  AssistantMediaSession assistant_media_session_{&media_host_};
};

TEST_F(AssistantMediaSessionTest, ShouldUpdateSessionStateOnStartStopDucking) {
  assistant_media_session()->StartDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateDucking());

  assistant_media_session()->StopDucking();
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());
}

TEST_F(AssistantMediaSessionTest,
       ShouldUpdateSessionStateAndSendActionOnSuspendResumePlaying) {
  // Suspend.
  EXPECT_CALL(libassistant_media_controller_mock(), PauseInternalMediaPlayer);
  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());

  // Then resume.
  EXPECT_CALL(libassistant_media_controller_mock(), ResumeInternalMediaPlayer);
  assistant_media_session()->Resume(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateActive());

  // And pause again.
  EXPECT_CALL(libassistant_media_controller_mock(), PauseInternalMediaPlayer);
  assistant_media_session()->Suspend(MediaSession::SuspendType::kSystem);
  EXPECT_TRUE(assistant_media_session()->IsSessionStateSuspended());
}

}  // namespace ash::assistant
