// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include <memory>

#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

struct WebApplicationInfo;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback);

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback);

}  // namespace web_app

// Consider to implement web app specific test harness independent of
// RenderViewHost.
using WebAppTest = ChromeRenderViewHostTestHarness;

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
