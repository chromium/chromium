// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_test.h"

class SiteEngagementBrowserTest : public WebUIMochaBrowserTest {
 public:
  SiteEngagementBrowserTest() {
    webui_omnibox_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
        // is enabled and then remove these two Features.
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup});
  }

 private:
  base::test::ScopedFeatureList webui_omnibox_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SiteEngagementBrowserTest, All) {
  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(
          browser()->profile());
  service->ResetBaseScoreForURL(GURL("http://example.com"), 10);
  service->ResetBaseScoreForURL(GURL("http://shmlexample.com/"), 3.14159);

  set_test_loader_host(chrome::kChromeUISiteEngagementHost);
  RunTest("engagement/site_engagement_test.js", "mocha.run()");
}
