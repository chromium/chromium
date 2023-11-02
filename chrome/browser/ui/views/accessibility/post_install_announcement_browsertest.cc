// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/test/ax_event_counter.h"

class PostInstallAnnouncementTestBase : public InProcessBrowserTest {
 protected:
  views::test::AXEventCounter event_counter_{views::AXEventManager::Get()};
};

IN_PROC_BROWSER_TEST_F(PostInstallAnnouncementTestBase, NormalLaunch) {
  // Expect no announcement from a normal launch.
  EXPECT_EQ(event_counter_.GetCount(ax::mojom::Event::kAlert), 0);
}

class PostInstallAnnouncementTest : public PostInstallAnnouncementTestBase {
 protected:
  // PostInstallAnnouncementTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Pretend that the browser was launched by the installer.
    command_line->AppendSwitch(switches::kFromInstaller);
  }
};

IN_PROC_BROWSER_TEST_F(PostInstallAnnouncementTest, FromInstaller) {
  // Expect that the welcome message was announced.
  EXPECT_EQ(event_counter_.GetCount(ax::mojom::Event::kAlert), 1);
}
