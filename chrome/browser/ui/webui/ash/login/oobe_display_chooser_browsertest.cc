// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/oobe_display_chooser.h"

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

class OobeDisplayChooserTest : public OobeBaseTest {
 public:
  OobeDisplayChooserTest() {}

  OobeDisplayChooserTest(const OobeDisplayChooserTest&) = delete;
  OobeDisplayChooserTest& operator=(const OobeDisplayChooserTest&) = delete;

  ~OobeDisplayChooserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);

    OobeBaseTest::SetUpCommandLine(command_line);
  }
};

display::DisplayManager* display_manager() {
  return Shell::Get()->display_manager();
}

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

}  // namespace

// Test that display removal does not trigger CHECK in
// WindowTreeHostManager::GetPrimaryDisplayId().
IN_PROC_BROWSER_TEST_F(OobeDisplayChooserTest,
                       RemovingPrimaryDisplaySanityCheck) {
  display::ManagedDisplayInfo info1(1, "x-1", false);
  info1.SetBounds(gfx::Rect(0, 0, 1280, 800));
  display::ManagedDisplayInfo info2(2, "x-2", false);
  std::vector<display::ManagedDisplayInfo> info_list;
  info2.SetBounds(gfx::Rect(0, 1280, 1280, 800));
  info_list.push_back(info1);
  info_list.push_back(info2);

  display_manager()->OnNativeDisplaysChanged(info_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetPrimaryDisplayId());

  info_list.erase(info_list.begin());
  display_manager()->OnNativeDisplaysChanged(info_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetPrimaryDisplayId());
}

}  // namespace ash
