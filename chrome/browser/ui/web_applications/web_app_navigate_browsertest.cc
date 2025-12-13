// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppServiceWorkerOpenWindowBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppServiceWorkerOpenWindowBrowserTest()
      : WebAppBrowserTestBase({}, {features::kPwaNavigationCapturing}) {}

  static GURL GetGoogleURL() { return GURL("http://www.google.com/"); }

  NavigateParams MakeNavigateParamsForServiceWorker() const {
    NavigateParams params(browser()->profile(), GetGoogleURL(),
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.is_renderer_initiated = true;
    params.user_gesture = false;
    params.initiator_origin = url::Origin::Create(GetGoogleURL());
    params.is_service_worker_open_window = true;
    return params;
  }
};

// This test verifies that navigating with "is_service_worker_open_window =
// true" opens a new app window if there is an installed Web App for the URL.
IN_PROC_BROWSER_TEST_F(WebAppServiceWorkerOpenWindowBrowserTest,
                       AppInstalled_ServiceWorkerOpenWindow) {
  InstallPWA(GetGoogleURL());

  NavigateParams params(MakeNavigateParamsForServiceWorker());
  Navigate(&params);

  EXPECT_NE(browser(), params.browser);
  EXPECT_FALSE(params.browser->GetBrowserForMigrationOnly()->is_type_normal());
  EXPECT_TRUE(params.browser->GetBrowserForMigrationOnly()->is_type_app());
  EXPECT_TRUE(
      params.browser->GetBrowserForMigrationOnly()->is_trusted_source());
}

IN_PROC_BROWSER_TEST_F(WebAppServiceWorkerOpenWindowBrowserTest,
                       AppNotInstalled_ServiceWorkerOpenWindow) {
  int num_tabs = browser()->tab_strip_model()->count();

  NavigateParams params(MakeNavigateParamsForServiceWorker());
  Navigate(&params);

  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
}

}  // namespace web_app
