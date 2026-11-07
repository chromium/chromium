// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using NearbyDeviceIconV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyDeviceIconV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_device_icon_test.js",
          "mocha.run()");
}

using NearbyDeviceV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyDeviceV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_device_test.js", "mocha.run()");
}

using NearbyOnboardingOnePageV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyOnboardingOnePageV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_onboarding_one_page_test.js",
          "mocha.run()");
}

using NearbyPageTemplateV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyPageTemplateV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_page_template_test.js",
          "mocha.run()");
}

using NearbyPreviewV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyPreviewV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_preview_test.js", "mocha.run()");
}

using NearbyProgressV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyProgressV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_progress_test.js",
          "mocha.run()");
}

using NearbyVisibilityPageV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyVisibilityPageV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_visibility_page_test.js",
          "mocha.run()");
}

using NearbyContactVisibilityV3Test = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(NearbyContactVisibilityV3Test, All) {
  set_test_loader_host(ash::kChromeUINearbyShareHost);
  RunTest("chromeos/nearby_share/shared/nearby_contact_visibility_test.js",
          "mocha.run()");
}
