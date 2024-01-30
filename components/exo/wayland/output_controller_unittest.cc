// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_controller.h"

#include <wayland-server-core.h>

#include "components/exo/test/exo_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/wayland/wayland_display_util.h"

namespace exo::wayland {

namespace {

class MockOutputControllerDelegate : public OutputController::Delegate {
 public:
  MockOutputControllerDelegate() : wayland_display_(wl_display_create()) {}

  // OutputController::Delegate:
  wl_display* GetWaylandDisplay() override { return wayland_display_; }
  MOCK_METHOD(void, Flush, (), (override));

 private:
  const raw_ptr<wl_display> wayland_display_;
};

}  // namespace

using OutputControllerTest = test::ExoTestBase;

TEST_F(OutputControllerTest, OutputControllerInitialization) {
  UpdateDisplay("800x600,800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());

  // OutputController should reflect the display state in display manager after
  // initialization.
  MockOutputControllerDelegate delegate;
  EXPECT_CALL(delegate, Flush()).Times(1);
  auto output_controller = std::make_unique<OutputController>(&delegate);
  EXPECT_EQ(2u, output_controller->outputs_for_testing().size());
  EXPECT_TRUE(output_controller->GetWaylandDisplayOutputForTesting(primary_id));
  EXPECT_TRUE(
      output_controller->GetWaylandDisplayOutputForTesting(secondary_id));
}

TEST_F(OutputControllerTest, OutputControllerRemoveDisplay) {
  UpdateDisplay("800x600,800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());

  MockOutputControllerDelegate delegate;
  EXPECT_CALL(delegate, Flush()).Times(2);
  auto output_controller = std::make_unique<OutputController>(&delegate);
  EXPECT_EQ(2u, output_controller->outputs_for_testing().size());
  EXPECT_TRUE(output_controller->GetWaylandDisplayOutputForTesting(primary_id));
  EXPECT_TRUE(
      output_controller->GetWaylandDisplayOutputForTesting(secondary_id));

  // Remove the secondary display.
  UpdateDisplay("800x600");
  EXPECT_EQ(1u, output_controller->outputs_for_testing().size());
  EXPECT_TRUE(output_controller->GetWaylandDisplayOutputForTesting(primary_id));
}

TEST_F(OutputControllerTest, OutputControllerAddDisplay) {
  UpdateDisplay("800x600");
  const auto* screen = display::Screen::GetScreen();
  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  ASSERT_EQ(1u, screen->GetAllDisplays().size());

  MockOutputControllerDelegate delegate;
  EXPECT_CALL(delegate, Flush()).Times(2);
  auto output_controller = std::make_unique<OutputController>(&delegate);
  EXPECT_EQ(1u, output_controller->outputs_for_testing().size());
  EXPECT_TRUE(output_controller->GetWaylandDisplayOutputForTesting(primary_id));

  // Add a second display.
  UpdateDisplay("800x600,800x600");
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());
  EXPECT_EQ(2u, output_controller->outputs_for_testing().size());
  EXPECT_TRUE(output_controller->GetWaylandDisplayOutputForTesting(primary_id));
  EXPECT_TRUE(
      output_controller->GetWaylandDisplayOutputForTesting(secondary_id));
}

}  // namespace exo::wayland
