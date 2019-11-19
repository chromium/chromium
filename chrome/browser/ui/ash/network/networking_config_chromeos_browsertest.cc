// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

using NetworkingConfigChromeosTest = extensions::ExtensionBrowserTest;

// Tests that an extension registering itself as handling a Wi-Fi SSID updates
// the ash system tray network item.
IN_PROC_BROWSER_TEST_F(NetworkingConfigChromeosTest, SystemTrayItem) {
  // Load the extension and wait for the background page script to run. This
  // registers the extension as the network config handler for wifi1.
  ExtensionTestMessageListener listener("done", false);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("networking_config")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Show the network detail view.
  auto tray_test_api = ash::SystemTrayTestApi::Create();
  tray_test_api->ShowNetworkDetailedView();
  base::RunLoop().RunUntilIdle();

  // Expect that the extension-controlled item appears.
  base::string16 expected_tooltip = base::UTF8ToUTF16(
      "The extension \"NetworkingConfig test extension\" can help connect to "
      "this network.");
  base::string16 tooltip = tray_test_api->GetBubbleViewTooltip(
      ash::VIEW_ID_EXTENSION_CONTROLLED_WIFI);
  EXPECT_EQ(expected_tooltip, tooltip);
}

}  // namespace
