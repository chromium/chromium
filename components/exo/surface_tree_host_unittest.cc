// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface_tree_host.h"

#include <memory>
#include <utility>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"

namespace exo {
namespace {

class SurfaceTreeHostTest : public test::ExoTestBase {
 protected:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    shell_surface_ = test::ShellSurfaceBuilder({16, 16}).BuildShellSurface();
  }

  void TearDown() override {
    shell_surface_.reset();

    test::ExoTestBase::TearDown();
  }

  ash::DisplayConfigurationController* display_config_controller() {
    return ash::Shell::Get()->display_configuration_controller();
  }

  std::unique_ptr<ShellSurface> shell_surface_;
};

}  // namespace

TEST_F(SurfaceTreeHostTest, UpdatePrimaryDisplayWithSurfaceUpdateFailure) {
  UpdateDisplay("800x600,1000x800@1.2");
  display::Display display1 = GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();

  std::vector<std::pair<int64_t, int64_t>> leave_enter_ids;
  bool callback_return_value = true;
  shell_surface_->root_surface()->set_leave_enter_callback(
      base::BindLambdaForTesting(
          [&leave_enter_ids, &callback_return_value](int64_t old_display_id,
                                                     int64_t new_display_id) {
            leave_enter_ids.emplace_back(old_display_id, new_display_id);
            return callback_return_value;
          }));

  // Successfully update surface to display 2.
  display_config_controller()->SetPrimaryDisplayId(display2.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 1u);
  EXPECT_EQ(leave_enter_ids[0], std::make_pair(display1.id(), display2.id()));

  // Fail to update surface to display 1.
  callback_return_value = false;
  display_config_controller()->SetPrimaryDisplayId(display1.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 2u);
  EXPECT_EQ(leave_enter_ids[1], std::make_pair(display2.id(), display1.id()));

  // Should still send an update for surface to enter display 2.
  callback_return_value = true;
  display_config_controller()->SetPrimaryDisplayId(display2.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 3u);
  EXPECT_EQ(leave_enter_ids[2],
            std::make_pair(display::kInvalidDisplayId, display2.id()));
}

TEST_F(SurfaceTreeHostTest,
       BuiltinDisplayMirrorModeToExtendModeWithExternalDisplayAsPrimary) {
  UpdateDisplay("800x600,1000x800@1.2");

  // Set first display as internal, so it'll be primary source in mirror mode.
  int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  int64_t external_display_id = GetSecondaryDisplay().id();

  ASSERT_NE(internal_display_id, external_display_id);

  std::vector<std::pair<int64_t, int64_t>> leave_enter_ids;
  shell_surface_->root_surface()->set_leave_enter_callback(
      base::BindLambdaForTesting(
          [&leave_enter_ids](int64_t old_display_id, int64_t new_display_id) {
            leave_enter_ids.emplace_back(old_display_id, new_display_id);
            return true;
          }));

  // Make external display primary.
  display_config_controller()->SetPrimaryDisplayId(external_display_id, false);

  ASSERT_EQ(leave_enter_ids.size(), 1u);
  EXPECT_EQ(leave_enter_ids[0],
            std::make_pair(internal_display_id, external_display_id));

  // Change to mirror mode, which should make internal display primary.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(leave_enter_ids.size(), 2u);
  EXPECT_EQ(leave_enter_ids[1],
            std::make_pair(external_display_id, internal_display_id));

  // Switch back to extend mode, which should restore external as primary.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(leave_enter_ids.size(), 3u);
  EXPECT_EQ(leave_enter_ids[2],
            std::make_pair(internal_display_id, external_display_id));
}

}  // namespace exo
