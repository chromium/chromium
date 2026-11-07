// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using NearbyConfirmationPageTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyConfirmationPageTest, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/nearby_confirmation_page_test.js",
          "mocha.run()");
}

using NearbyDiscoveryPageTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyDiscoveryPageTest, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/nearby_discovery_page_test.js", "mocha.run()");
}

using NearbyShareAppTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyShareAppTest, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/nearby_share_app_test.js", "mocha.run()");
}
