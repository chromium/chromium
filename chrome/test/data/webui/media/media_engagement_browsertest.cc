// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

class MediaEngagementWebUIBrowserTest : public WebUIMochaBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      media::kRecordMediaEngagementScores};
};

IN_PROC_BROWSER_TEST_F(MediaEngagementWebUIBrowserTest, All) {
  MediaEngagementService* service =
      MediaEngagementServiceFactory::GetForProfile(browser()->profile());
  MediaEngagementScore score1 = service->CreateEngagementScore(
      url::Origin::Create(GURL("http://example.com")));
  score1.IncrementVisits();
  score1.IncrementMediaPlaybacks();
  score1.Commit();
  MediaEngagementScore score2 = service->CreateEngagementScore(
      url::Origin::Create(GURL("http://shmlexample.com/")));
  score2.IncrementVisits();
  score2.IncrementMediaPlaybacks();
  score2.Commit();

  set_test_loader_host(chrome::kChromeUIMediaEngagementHost);
  RunTestWithoutTestLoader("media/media_engagement_test.js", "mocha.run()");
}
