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

  browsePreload: 'chrome://site-engagement',

  isAsync: true,

  testGenPreamble: function() {
    GEN('site_engagement::SiteEngagementService* service =');
    GEN('    site_engagement::SiteEngagementServiceFactory::GetForProfile(browser()->profile());');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_1 + '"), 10);');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_2 +
        '"), 3.14159);');
  },

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('SiteEngagementBrowserTest', 'All', function() {
  let app;
  let cells;

  function getCells() {
    const originCells =
        Array.from(app.shadowRoot.querySelectorAll('.origin-cell'));
    const scoreInputs =
        Array.from(app.shadowRoot.querySelectorAll('.base-score-input'));
    const bonusScoreCells =
        Array.from(app.shadowRoot.querySelectorAll('.bonus-score-cell'));
    const totalScoreCells =
        Array.from(app.shadowRoot.querySelectorAll('.total-score-cell'));
    return originCells.map((c, i) => {
      return {
        origin: c,
        scoreInput: scoreInputs[i],
        bonusScore: bonusScoreCells[i],
        totalScore: totalScoreCells[i],
      };
    });
  }

  setup(async function() {
    await import('chrome://webui-test/mojo_webui_test_support.js');
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    app = document.createElement('site-engagement-app');
    document.body.appendChild(app);
    await app.whenPopulatedForTest();
    app.disableAutoupdate();
    cells = getCells();
  });

  test('check engagement values are loaded', function() {
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2], cells.map((c) => c.origin.textContent));
  });

  test('scores rounded to 2 decimal places', function() {
    assertDeepEquals(['10', '3.14'], cells.map((x) => x.scoreInput.value));
    assertDeepEquals(['0', '0'], cells.map((x) => x.bonusScore.textContent));
    assertDeepEquals(
        ['10', '3.14'], cells.map((x) => x.totalScore.textContent));
  });

  test('change score', async function() {
    const firstRow = cells[0];
    firstRow.scoreInput.value = 50;
    firstRow.scoreInput.dispatchEvent(new Event('change'));

    const {info} =
        await app.engagementDetailsProvider.getSiteEngagementDetails();
    assertEquals(firstRow.origin.textContent, info[0].origin.url);
    assertEquals(50, info[0].baseScore);
  });
  mocha.run();
});
