// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

class ScreenAIInstallStateTest : public testing::Test,
                                 ScreenAIInstallState::Observer {
 public:
  ScreenAIInstallStateTest() {
    ScreenAIInstallState::GetInstance()->ResetForTesting();
  }

  void StartObservation() {
    component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  void MakeComponentReady() {
    // The passed file path is not used and just indicates that the component
    // exists.
    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(FILE_PATH_LITERAL("tmp")));
  }

  void StateChanged(ScreenAIInstallState::State state) override {
    if (state == ScreenAIInstallState::State::kReady)
      component_ready_received_ = true;
  }

  bool ComponentReadyReceived() { return component_ready_received_; }

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  bool component_ready_received_ = false;
};

TEST_F(ScreenAIInstallStateTest, NeverReady) {
  StartObservation();
  EXPECT_FALSE(ComponentReadyReceived());
}

TEST_F(ScreenAIInstallStateTest, ReadyBeforeObservation) {
  MakeComponentReady();
  StartObservation();
  EXPECT_TRUE(ComponentReadyReceived());
}

TEST_F(ScreenAIInstallStateTest, ReadyAfterObservation) {
  StartObservation();
  MakeComponentReady();
  EXPECT_TRUE(ComponentReadyReceived());
}

}  // namespace screen_ai
