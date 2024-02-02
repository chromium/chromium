// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_controller.h"

#include <wayland-server-core.h>

#include "components/exo/wayland/output_controller_test_api.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/wayland/wayland_display_util.h"

namespace exo::wayland {

namespace {

class MockOutputControllerDelegate : public OutputController::Delegate {
 public:
  MockOutputControllerDelegate() : wayland_display_(wl_display_create()) {}
  ~MockOutputControllerDelegate() override {
    wl_display_destroy(wayland_display_.ExtractAsDangling());
  }

  // OutputController::Delegate:
  wl_display* GetWaylandDisplay() override { return wayland_display_; }
  MOCK_METHOD(void, Flush, (), (override));

 private:
  raw_ptr<wl_display> wayland_display_;
};

}  // namespace

class OutputControllerTest : public test::WaylandServerTest {
 public:
  OutputControllerTest() = default;
  OutputControllerTest(const OutputControllerTest&) = delete;
  OutputControllerTest& operator=(const OutputControllerTest&) = delete;
  ~OutputControllerTest() override = default;

 protected:
  MockOutputControllerDelegate delegate_;
};

TEST_F(OutputControllerTest, OutputControllerInitialization) {
  UpdateDisplay("800x600,800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());

  // OutputController should reflect the display state in display manager after
  // initialization.
  EXPECT_CALL(delegate_, Flush()).Times(1);
  OutputController output_controller(&delegate_);
  OutputControllerTestApi output_controller_test_api(output_controller);
  EXPECT_EQ(2u, output_controller_test_api.GetOutputMap().size());
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(primary_id));
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(secondary_id));
}

TEST_F(OutputControllerTest, OutputControllerRemoveDisplay) {
  UpdateDisplay("800x600,800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());

  EXPECT_CALL(delegate_, Flush()).Times(2);
  OutputController output_controller(&delegate_);
  OutputControllerTestApi output_controller_test_api(output_controller);
  EXPECT_EQ(2u, output_controller_test_api.GetOutputMap().size());
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(primary_id));
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(secondary_id));

  // Remove the secondary display.
  UpdateDisplay("800x600");
  EXPECT_EQ(1u, output_controller_test_api.GetOutputMap().size());
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(primary_id));
}

TEST_F(OutputControllerTest, OutputControllerAddDisplay) {
  UpdateDisplay("800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  ASSERT_EQ(1u, screen->GetAllDisplays().size());

  EXPECT_CALL(delegate_, Flush()).Times(2);
  OutputController output_controller(&delegate_);
  OutputControllerTestApi output_controller_test_api(output_controller);
  EXPECT_EQ(1u, output_controller_test_api.GetOutputMap().size());
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(primary_id));

  // Add a second display.
  UpdateDisplay("800x600,800x600");
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());
  EXPECT_EQ(2u, output_controller_test_api.GetOutputMap().size());
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(primary_id));
  EXPECT_TRUE(output_controller_test_api.GetWaylandDisplayOutput(secondary_id));
}

// Regression test for b/323403137. Tests that exo will respect display
// activation events in the case observers trigger activation updates before the
// controller has had the opportunity to update output state.
TEST_F(OutputControllerTest, ActiveDisplay) {
  // Activates a second display when added to the system.
  class SecondaryDisplayActivator : public display::DisplayManagerObserver {
   public:
    SecondaryDisplayActivator() {
      display_manager_observation_.Observe(
          ash::Shell::Get()->display_manager());
    }
    // display::DisplayManagerObserver:
    void OnDidProcessDisplayChanges(
        const DisplayConfigurationChange& configuration_change) override {
      auto* screen = display::Screen::GetScreen();
      EXPECT_EQ(2u, screen->GetAllDisplays().size());
      screen->SetDisplayForNewWindows(screen->GetAllDisplays()[1].id());
    }

   private:
    base::ScopedObservation<display::DisplayManager,
                            display::DisplayManagerObserver>
        display_manager_observation_{this};
  };

  // Setup the environment with a single display.
  UpdateDisplay("800x600");
  auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  ASSERT_EQ(1u, screen->GetAllDisplays().size());
  OutputController output_controller(&delegate_);
  OutputControllerTestApi output_controller_test_api(output_controller);

  // Force an activation notification for the primary display.
  screen->SetDisplayForNewWindows(primary_id);
  EXPECT_EQ(primary_id,
            output_controller_test_api.GetDispatchedActivatedDisplayId());

  // Update the display manager observer ordering such that the display
  // activator is notified before the output controller.
  SecondaryDisplayActivator secondary_display_activator;
  output_controller_test_api.ResetDisplayManagerObservation();

  // Add a secondary display. The display activator will send a display
  // activation notification before the controller processes the display update.
  // Make sure the activation event is preserved.
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2u, screen->GetAllDisplays().size());
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  EXPECT_EQ(secondary_id,
            output_controller_test_api.GetDispatchedActivatedDisplayId());
}

}  // namespace exo::wayland
