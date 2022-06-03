// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Media Engagement WebUI.
 */
var EXAMPLE_URL_1 = 'http://example.com';
var EXAMPLE_URL_2 = 'http://shmlexample.com';

GEN('#include "chrome/browser/media/media_engagement_service.h"');
GEN('#include "chrome/browser/media/media_engagement_service_factory.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "media/base/media_switches.h"');

function MediaEngagementWebUIBrowserTest() {}

MediaEngagementWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-engagement',

  featureList: {enabled: ['media::kRecordMediaEngagementScores']},

  isAsync: true,

  testGenPreamble: function() {
    GEN('MediaEngagementService* service =');
    GEN('  MediaEngagementServiceFactory::GetForProfile(');
    GEN('    browser()->profile());');
    GEN('MediaEngagementScore score1 = service->CreateEngagementScore(');
    GEN('     url::Origin::Create(GURL("' + EXAMPLE_URL_1 + '")));');
    GEN('score1.IncrementVisits();');
    GEN('score1.IncrementMediaPlaybacks();');
    GEN('score1.Commit();');
    GEN('MediaEngagementScore score2 = service->CreateEngagementScore(');
    GEN('     url::Origin::Create(GURL("' + EXAMPLE_URL_2 + '")));');
    GEN('score2.IncrementVisits();');
    GEN('score2.IncrementMediaPlaybacks();');
    GEN('score2.Commit();');
  },

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('MediaEngagementWebUIBrowserTest', 'All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check engagement values are loaded', function() {
    var originCells =
        Array.from(document.getElementsByClassName('origin-cell'));
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2], originCells.map(x => x.textContent));
  });

  mocha.run();
});
