// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/chromebox_for_meetings/network_settings_dialog.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class NetworkSettingsDialogTest : public InProcessBrowserTest {};

// Verifies dialog creation and startup. Dialog webui / JS tested separately.
IN_PROC_BROWSER_TEST_F(NetworkSettingsDialogTest, ShowTest) {
  chromeos::cfm::NetworkSettingsDialog::ShowDialog();
  EXPECT_TRUE(chromeos::cfm::NetworkSettingsDialog::IsShown());
}
