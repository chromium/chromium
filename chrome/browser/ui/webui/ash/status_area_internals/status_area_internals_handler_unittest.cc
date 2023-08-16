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
#include "components/prefs/pref_service.h"
#include "content/public/test/test_web_ui.h"
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
    handler_ = std::make_unique<StatusAreaInternalsHandler>();
    handler_->SetWebUiForTesting(&web_ui_);
    handler_->RegisterMessages();

    AshTestBase::SetUp();
  }

  void SendMessage(const std::string& handler_name,
                   const base::Value::List& args) {
    web_ui_.HandleReceivedMessage(handler_name, args);
    task_environment()->RunUntilIdle();
  }

  StatusAreaWidget* GetStatusAreaWidget() {
    return ash::Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget();
  }

 private:
  content::TestWebUI web_ui_;

  std::unique_ptr<StatusAreaInternalsHandler> handler_;
};

// Sending `kToggleIme` message from the web UI should update the visibility of
// IME tray accordingly.
TEST_F(StatusAreaInternalsHandlerTest, ToggleImeTray) {
  auto* ime_tray = GetStatusAreaWidget()->ime_menu_tray();
  EXPECT_FALSE(ime_tray->GetVisible());

  base::Value::List args;
  args.Append(true);
  SendMessage(StatusAreaInternalsHandler::kToggleIme, args);

  EXPECT_TRUE(ime_tray->GetVisible());

  args.clear();
  args.Append(false);
  SendMessage(StatusAreaInternalsHandler::kToggleIme, args);

  EXPECT_FALSE(ime_tray->GetVisible());
}

// Sending `kTogglePalette` message from the web UI should update the visibility
// of palette tray accordingly.
TEST_F(StatusAreaInternalsHandlerTest, TogglePaletteTray) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kEnableStylusTools, true);

  auto* palette_tray = GetStatusAreaWidget()->palette_tray();
  EXPECT_FALSE(palette_tray->GetVisible());

  base::Value::List args;
  args.Append(true);
  SendMessage(StatusAreaInternalsHandler::kTogglePalette, args);

  EXPECT_TRUE(palette_tray->GetVisible());

  args.clear();
  args.Append(false);
  SendMessage(StatusAreaInternalsHandler::kTogglePalette, args);

  EXPECT_FALSE(palette_tray->GetVisible());
}

// Sending `kTriggerPrivacyIndicators` message from the web UI should update the
// visibility of the privacy indicators accordingly.
TEST_F(StatusAreaInternalsHandlerTest, TriggerPrivacyIndicators) {
  auto* privacy_indicators_view = GetStatusAreaWidget()
                                      ->notification_center_tray()
                                      ->privacy_indicators_view();
  ASSERT_FALSE(privacy_indicators_view->GetVisible());

  base::Value::List args;
  args.Append("app_id");
  args.Append("app_name");
  args.Append(true);
  args.Append(true);
  SendMessage(StatusAreaInternalsHandler::kTriggerPrivacyIndicators, args);

  EXPECT_TRUE(privacy_indicators_view->GetVisible());

  args.clear();
  args.Append("app_id");
  args.Append("app_name");
  args.Append(false);
  args.Append(false);
  SendMessage(StatusAreaInternalsHandler::kTriggerPrivacyIndicators, args);

  EXPECT_FALSE(privacy_indicators_view->GetVisible());
}

}  // namespace ash
