// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/timer_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/mojom/timer_controller.mojom-forward.h"
#include "chromeos/ash/services/libassistant/test_support/fake_assistant_client.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

class TimerDelegateMock : public mojom::TimerDelegate {
 public:
  TimerDelegateMock() = default;
  TimerDelegateMock(const TimerDelegateMock&) = delete;
  TimerDelegateMock& operator=(const TimerDelegateMock&) = delete;
  ~TimerDelegateMock() override = default;

  // mojom::TimerDelegate implementation:
  MOCK_METHOD(void,
              OnTimerStateChanged,
              (const std::vector<assistant::AssistantTimer>& timers));

  mojo::PendingRemote<mojom::TimerDelegate> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::TimerDelegate> receiver_{this};
};

}  // namespace

class AssistantTimerControllerTest : public ::testing::Test {
 public:
  AssistantTimerControllerTest() = default;
  AssistantTimerControllerTest(const AssistantTimerControllerTest&) = delete;
  AssistantTimerControllerTest& operator=(const AssistantTimerControllerTest&) =
      delete;
  ~AssistantTimerControllerTest() override = default;

  void SetUp() override {
    controller_.Bind(client_.BindNewPipeAndPassReceiver(),
                     delegate_.BindNewPipeAndPassRemote());
    Init();
  }

  void Init() {
    auto assistant_manager =
        std::make_unique<chromeos::assistant::FakeAssistantManager>();
    assistant_client_ =
        std::make_unique<FakeAssistantClient>(std::move(assistant_manager));
  }

  void StartLibassistant() {
    if (!assistant_client_) {
      Init();
    }
    controller_.OnAssistantClientRunning(assistant_client_.get());
  }

  void StopLibassistant() {
    controller_.OnDestroyingAssistantClient(assistant_client_.get());

    // Delete the assistant manager so we crash on use-after-free.
    assistant_client_.reset();
  }

  TimerController& controller() { return controller_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  std::unique_ptr<FakeAssistantClient> assistant_client_;
  mojo::Remote<mojom::TimerController> client_;
  testing::StrictMock<TimerDelegateMock> delegate_;
  TimerController controller_;
};

TEST_F(AssistantTimerControllerTest, ShouldNotCrashIfLibassistantIsNotStarted) {
  controller().AddTimeToTimer("timer-id", /*duration=*/base::TimeDelta());
  controller().PauseTimer("timer-id");
  controller().RemoveTimer("timer-id");
  controller().ResumeTimer("timer-id");
}

TEST_F(AssistantTimerControllerTest, ShouldNotCrashAfterStoppingLibassistant) {
  StartLibassistant();
  StopLibassistant();

  controller().AddTimeToTimer("timer-id", /*duration=*/base::TimeDelta());
  controller().PauseTimer("timer-id");
  controller().RemoveTimer("timer-id");
  controller().ResumeTimer("timer-id");
}

}  // namespace ash::libassistant
