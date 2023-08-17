// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Site Engagement WebUI.
 */
const EXAMPLE_URL_1 = 'http://example.com/';
const EXAMPLE_URL_2 = 'http://shmlexample.com/';

GEN('#include "components/site_engagement/content/site_engagement_service.h"');
GEN('#include "chrome/browser/engagement/site_engagement_service_factory.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "content/public/test/browser_test.h"');

function SiteEngagementBrowserTest() {}

SiteEngagementBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload:
      'chrome://site-engagement/test_loader.html?module=engagement/site_engagement_test.js',

  isAsync: true,

  testGenPreamble: function() {
    GEN('site_engagement::SiteEngagementService* service =');
    GEN('    site_engagement::SiteEngagementServiceFactory::GetForProfile(browser()->profile());');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_1 + '"), 10);');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_2 +
        '"), 3.14159);');
  },
};

TEST_F('SiteEngagementBrowserTest', 'All', function() {
  mocha.run();
});
