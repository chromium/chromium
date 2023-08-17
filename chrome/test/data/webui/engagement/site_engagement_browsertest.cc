// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_test.h"

using SiteEngagementBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(SiteEngagementBrowserTest, All) {
  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(
          browser()->profile());
  service->ResetBaseScoreForURL(GURL("http://example.com"), 10);
  service->ResetBaseScoreForURL(GURL("http://shmlexample.com/"), 3.14159);

  set_test_loader_host(chrome::kChromeUISiteEngagementHost);
  RunTest("engagement/site_engagement_test.js", "mocha.run()");
}
