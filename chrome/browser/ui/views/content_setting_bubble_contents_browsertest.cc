// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class ContentSettingBubbleContentsBrowserTest : public InProcessBrowserTest {
 public:
  ContentSettingBubbleContentsBrowserTest() = default;
  ~ContentSettingBubbleContentsBrowserTest() override = default;

 protected:
  GURL GetTestPageUrl(const std::string& name) {
    return ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("content_setting_bubble"),
        base::FilePath().AppendASCII(name));
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Flaky: https://crbug.com/1073516
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleContentsBrowserTest,
                       DISABLED_HidesAtWebContentsClose) {
  // Create a second tab, so closing the test tab doesn't close the browser.
  chrome::NewTab(browser());

  // Navigate to the test page, and have it request and be denied geolocation
  // permissions.
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(),
                                     GetTestPageUrl("geolocation.html")));
  permissions::PermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DISMISS);
  ASSERT_TRUE(content::ExecJs(GetWebContents(), "geolocate();"));
  permissions::PermissionRequestObserver(GetWebContents()).Wait();

  // Press the geolocation icon and make sure its content setting bubble shows.
  LocationBarTesting* bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  EXPECT_TRUE(bar->TestContentSettingImagePressed(
      static_cast<size_t>(ContentSettingImageModel::ImageType::GEOLOCATION)));
  EXPECT_TRUE(bar->IsContentSettingBubbleShowing(
      static_cast<size_t>(ContentSettingImageModel::ImageType::GEOLOCATION)));

  // Close the tab, and make sure the bubble is gone. Note that window closure
  // in Aura is asynchronous, so it's necessary to spin the run loop here.
  chrome::CloseTab(browser());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bar->IsContentSettingBubbleShowing(
      static_cast<size_t>(ContentSettingImageModel::ImageType::GEOLOCATION)));
}
