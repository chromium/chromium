// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchQuery, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

export const MIN_RELATED_SEARCHES = 3;

export function assertModuleHeaderTitle(
    headerElement: HTMLElement, title: string) {
  const moduleHeaderTextContent = headerElement.textContent!.trim();
  const headerText = moduleHeaderTextContent.split(/\r?\n/);
  assertTrue(headerText.length > 0);
  assertEquals(title, headerText[0]!.trim());
}

export function createVisit(overrides?: Partial<URLVisit>): URLVisit {
  return Object.assign(
      {
        visitId: BigInt(1),
        normalizedUrl: {url: 'https://www.foo.com'},
        urlForDisplay: 'www.foo.com',
        pageTitle: 'Test Title',
        titleMatchPositions: [],
        urlForDisplayMatchPositions: [],
        duplicates: [],
        relativeDate: '',
        annotations: [],
        debugInfo: {},
        rawVisitData: {
          url: {url: ''},
          visitTime: {internalValue: BigInt(0)},
        },
        hasUrlKeyedImage: false,
        isKnownToSync: true,
      },
      overrides);
}

export const GOOGLE_SEARCH_BASE_URL = 'https://www.google.com/search';

// Use Layout 1 as default for tests that do not care which layout.
export function createSampleVisits(
    numVisits: number = LAYOUT_1_MIN_VISITS,
    numImageVisits: number = LAYOUT_1_MIN_IMAGE_VISITS,
    imageVisitsFirst: boolean = true): URLVisit[] {
  const nonSrpVisits = Array(numVisits).fill(0).map((_, i) => {
    const id = i + 1;
    return createVisit({
      visitId: BigInt(id),
      normalizedUrl: {url: `https://www.foo.com/${id}`},
      urlForDisplay: `www.foo.com/${id}`,
      pageTitle: `Test Title ${id}`,
      hasUrlKeyedImage: i < numImageVisits,
    });
  });
  if (!imageVisitsFirst) {
    nonSrpVisits.reverse();
  }

  return [
    createVisit({
      visitId: BigInt(0),
      normalizedUrl: {url: `${GOOGLE_SEARCH_BASE_URL}?q=foo`},
      urlForDisplay: 'www.google.com',
      pageTitle: 'SRP',
    }),
    ...nonSrpVisits,
  ];
}

export function createRelatedSearches(num: number = MIN_RELATED_SEARCHES):
    SearchQuery[] {
  const result: SearchQuery[] = [];

  for (let i = 0; i < num; i++) {
    result.push({
      query: `Test Query ${i}`,
      url: {
        url: `${GOOGLE_SEARCH_BASE_URL}?q=${encodeURIComponent(`test${i}`)}`,
      },
    });
  }
  return result;
}
