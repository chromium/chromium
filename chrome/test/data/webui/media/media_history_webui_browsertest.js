// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Media History WebUI.
 */

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "media/base/media_switches.h"');

function MediaHistoryWebUIBrowserTest() {}

MediaHistoryWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  featureList: {enabled: ['media::kUseMediaHistoryStore']},

  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

// https://crbug.com/1045500: Flaky on Windows.
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

/**
 * Tests for the stats tab.
 * @extends {MediaHistoryWebUIBrowserTest}
 */
function MediaHistoryStatsWebUIBrowserTest() {}

MediaHistoryStatsWebUIBrowserTest.prototype = {
  __proto__: MediaHistoryWebUIBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://media-history#tab-stats',
};

TEST_F('MediaHistoryStatsWebUIBrowserTest', 'MAYBE_All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check stats table is loaded', () => {
    let statsRows =
        Array.from(document.getElementById('stats-table-body').children);

    assertDeepEquals(
        [
          ['mediaImage', '0'],
          ['meta', '3'],
          ['origin', '0'],
          ['playback', '0'],
          ['playbackSession', '0'],
          ['sessionImage', '0'],
        ],
        statsRows.map(
            x => [x.children[0].textContent, x.children[1].textContent]));
  });

  mocha.run();
});

/**
 * Tests for the origins tab.
 * @extends {MediaHistoryWebUIBrowserTest}
 */
function MediaHistoryOriginsWebUIBrowserTest() {}

MediaHistoryOriginsWebUIBrowserTest.prototype = {
  __proto__: MediaHistoryWebUIBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://media-history#tab-origins',
};

TEST_F('MediaHistoryOriginsWebUIBrowserTest', 'MAYBE_All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check data table is loaded', () => {
    let dataHeaderRows =
        Array.from(document.querySelector('#origins-table thead tr').children);

    assertDeepEquals(
        [
          'Origin', 'Last Updated', 'Audio + Video Watchtime (secs, cached)',
          'Audio + Video Watchtime (secs, actual)'
        ],
        dataHeaderRows.map(x => x.textContent.trim()));
  });

  mocha.run();
});

/**
 * Tests for the playbacks tab.
 * @extends {MediaHistoryWebUIBrowserTest}
 */
function MediaHistoryPlaybacksWebUIBrowserTest() {}

MediaHistoryPlaybacksWebUIBrowserTest.prototype = {
  __proto__: MediaHistoryWebUIBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://media-history#tab-playbacks',
};

TEST_F('MediaHistoryPlaybacksWebUIBrowserTest', 'MAYBE_All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check data table is loaded', () => {
    let dataHeaderRows = Array.from(
        document.querySelector('#playbacks-table thead tr').children);

    assertDeepEquals(
        ['URL', 'Last Updated', 'Has Audio', 'Has Video', 'Watchtime (secs)'],
        dataHeaderRows.map(x => x.textContent.trim()));
  });

  mocha.run();
});

/**
 * Tests for the sessions tab.
 * @extends {MediaHistoryWebUIBrowserTest}
 */
function MediaHistorySessionsWebUIBrowserTest() {}

MediaHistorySessionsWebUIBrowserTest.prototype = {
  __proto__: MediaHistoryWebUIBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://media-history#tab-sessions',
};

TEST_F('MediaHistorySessionsWebUIBrowserTest', 'MAYBE_All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check data table is loaded', () => {
    let dataHeaderRows =
        Array.from(document.querySelector('#sessions-table thead tr').children);

    assertDeepEquals(
        [
          'URL', 'Last Updated', 'Position (secs)', 'Duration (secs)', 'Title',
          'Artist', 'Album', 'Source Title', 'Artwork'
        ],
        dataHeaderRows.map(x => x.textContent.trim()));
  });

  mocha.run();
});
