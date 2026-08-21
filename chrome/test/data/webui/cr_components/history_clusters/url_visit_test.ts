// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_clusters/url_visit.js';

import type {URLVisit} from 'chrome://resources/cr_components/history_clusters/history_cluster_types.mojom-webui.js';
import type {UrlVisitElement} from 'chrome://resources/cr_components/history_clusters/url_visit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const SHORT_URL = 'https://a.co';
const LONG_URL =
    'https://www.example.com/a/very/long/path/that/is/expected/to/be/truncated/in/the/grouped/history/side/panel';
const UPDATED_URL = 'https://new.example/path';

function createTestVisit(urlForDisplay: string): URLVisit {
  return {
    visitId: BigInt(1),
    normalizedUrl: urlForDisplay,
    urlForDisplay,
    pageTitle: 'Example title',
    titleMatchPositions: [],
    urlForDisplayMatchPositions: [],
    duplicates: [],
    relativeDate: '',
    annotations: [],
    debugInfo: {},
    rawVisitData: {
      url: urlForDisplay,
      visitTime: {internalValue: BigInt(0)},
    },
    isKnownToSync: false,
    hasUrlKeyedImage: false,
  };
}

suite('UrlVisitTest', () => {
  let element: UrlVisitElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({inSidePanel: true});
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('url-visit');
    element.visit = createTestVisit(SHORT_URL);
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('ShowsTooltipForShortUrl', () => {
    assertEquals(SHORT_URL, element.$.url.title);
  });

  test('ShowsTooltipForLongUrl', async () => {
    element.visit = createTestVisit(LONG_URL);
    await microtasksFinished();

    assertEquals(LONG_URL, element.$.url.title);
  });

  test('UpdatesTooltipWhenVisitChanges', async () => {
    element.$.url.title = SHORT_URL;
    element.visit = createTestVisit(UPDATED_URL);

    await microtasksFinished();

    assertEquals(UPDATED_URL, element.$.url.title);
  });
});
