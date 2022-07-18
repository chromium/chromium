// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/display/mac/test/virtual_display_mac_util.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

class VirtualDisplayMacUtilInteractiveUitest : public InProcessBrowserTest {
 protected:
  VirtualDisplayMacUtilInteractiveUitest() = default;

  VirtualDisplayMacUtilInteractiveUitest(
      const VirtualDisplayMacUtilInteractiveUitest&) = delete;
  VirtualDisplayMacUtilInteractiveUitest& operator=(
      const VirtualDisplayMacUtilInteractiveUitest&) = delete;

  void SetUp() override {
    if (!display::test::VirtualDisplayMacUtil::IsAPIAvailable()) {
      GTEST_SKIP() << "Skipping test for MacOS 10.13 and older or Arm Macs.";
    }

    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(VirtualDisplayMacUtilInteractiveUitest, WarmUp) {
  int screen_number = display::Screen::GetScreen()->GetNumDisplays();

  display::test::VirtualDisplayMacUtil virtual_display_mac_util;
  virtual_display_mac_util.WarmUp();

  DCHECK_EQ(display::Screen::GetScreen()->GetNumDisplays(), screen_number);
}

IN_PROC_BROWSER_TEST_F(VirtualDisplayMacUtilInteractiveUitest, AddDisplay) {
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;
  virtual_display_mac_util.WarmUp();

  int64_t id = virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  DCHECK_NE(id, display::kInvalidDisplayId);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  DCHECK(found);
}

IN_PROC_BROWSER_TEST_F(VirtualDisplayMacUtilInteractiveUitest, RemoveDisplay) {
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;
  virtual_display_mac_util.WarmUp();

  int64_t id = virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  int screen_number = display::Screen::GetScreen()->GetNumDisplays();
  DCHECK(screen_number >= 1);

  virtual_display_mac_util.RemoveDisplay(id);
  DCHECK_EQ(display::Screen::GetScreen()->GetNumDisplays(), screen_number - 1);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  DCHECK(!found);
}

IN_PROC_BROWSER_TEST_F(VirtualDisplayMacUtilInteractiveUitest, IsAPIAvailable) {
  DCHECK(display::test::VirtualDisplayMacUtil::IsAPIAvailable());
}

IN_PROC_BROWSER_TEST_F(VirtualDisplayMacUtilInteractiveUitest, HotPlug) {
  int screen_number = display::Screen::GetScreen()->GetNumDisplays();

  std::unique_ptr<display::test::VirtualDisplayMacUtil>
      virtual_display_mac_util =
          std::make_unique<display::test::VirtualDisplayMacUtil>();
  virtual_display_mac_util->WarmUp();

  virtual_display_mac_util->AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  DCHECK_EQ(display::Screen::GetScreen()->GetNumDisplays(), screen_number + 1);

  virtual_display_mac_util->AddDisplay(
      2, display::test::VirtualDisplayMacUtil::k1920x1080);
  DCHECK_EQ(display::Screen::GetScreen()->GetNumDisplays(), screen_number + 2);

  virtual_display_mac_util.reset();
  DCHECK_EQ(display::Screen::GetScreen()->GetNumDisplays(), screen_number);
}
