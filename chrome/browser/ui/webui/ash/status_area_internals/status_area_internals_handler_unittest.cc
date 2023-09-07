// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_handler.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/ui/webui/ash/status_area_internals/mojom/status_area_internals.mojom.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class StatusAreaInternalsHandlerTest : public AshTestBase {
 public:
  StatusAreaInternalsHandlerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  StatusAreaInternalsHandlerTest(const StatusAreaInternalsHandlerTest&) =
      delete;
  StatusAreaInternalsHandlerTest& operator=(
      const StatusAreaInternalsHandlerTest&) = delete;
  ~StatusAreaInternalsHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<StatusAreaInternalsHandler>(
        handler_remote_.BindNewPipeAndPassReceiver());

    AshTestBase::SetUp();
  }

  StatusAreaWidget* GetStatusAreaWidget() {
    return ash::Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget();
  }

  const mojo::Remote<mojom::status_area_internals::PageHandler>&
  handler_remote() {
    return handler_remote_;
  }

 private:
  mojo::Remote<mojom::status_area_internals::PageHandler> handler_remote_;

  std::unique_ptr<StatusAreaInternalsHandler> handler_;
};

// Trigger `ToggleImeTray` from the test web UI remote should update the
// visibility of IME tray accordingly.
TEST_F(StatusAreaInternalsHandlerTest, ToggleImeTray) {
  auto* ime_tray = GetStatusAreaWidget()->ime_menu_tray();
  EXPECT_FALSE(ime_tray->GetVisible());

  handler_remote()->ToggleImeTray(/*visible=*/true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(ime_tray->GetVisible());

  handler_remote()->ToggleImeTray(/*visible=*/false);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(ime_tray->GetVisible());
}

// Trigger `TogglePaletteTray` from the test web UI remote should update the
// visibility of palette tray accordingly.
TEST_F(StatusAreaInternalsHandlerTest, TogglePaletteTray) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kEnableStylusTools, true);

  auto* palette_tray = GetStatusAreaWidget()->palette_tray();
  EXPECT_FALSE(palette_tray->GetVisible());

  handler_remote()->TogglePaletteTray(/*visible=*/true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(palette_tray->GetVisible());

  handler_remote()->TogglePaletteTray(/*visible=*/false);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(palette_tray->GetVisible());
}

// Trigger `TriggerPrivacyIndicators` from the test web UI remote should update
// the visibility of the privacy indicators accordingly.
TEST_F(StatusAreaInternalsHandlerTest, TriggerPrivacyIndicators) {
  auto* privacy_indicators_view = GetStatusAreaWidget()
                                      ->notification_center_tray()
                                      ->privacy_indicators_view();
  ASSERT_FALSE(privacy_indicators_view->GetVisible());

  handler_remote()->TriggerPrivacyIndicators("app_id", "app_name",
                                             /*is_camera_used=*/true,
                                             /*is_microphone_used=*/true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(privacy_indicators_view->GetVisible());

  handler_remote()->TriggerPrivacyIndicators("app_id", "app_name",
                                             /*is_camera_used=*/false,
                                             /*is_microphone_used=*/false);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(privacy_indicators_view->GetVisible());
}

}  // namespace ash
