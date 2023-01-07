// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

class ScreenAIInstallStateTest : public testing::Test,
                                 ScreenAIInstallState::Observer {
 public:
  void StartObservation() {
    component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  void MakeComponentReady() {
    ScreenAIInstallState::GetInstance()->SetComponentReady();
  }

  void ComponentReady() override { component_ready_received_ = true; }

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
