// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchQuery, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS} from 'chrome://new-tab-page/lazy_load.js';

export const MIN_RELATED_SEARCHES = 3;

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
    numImageVisits: number = LAYOUT_1_MIN_IMAGE_VISITS): URLVisit[] {
  const result: URLVisit[] = [];

  // Create SRP visit.
  result.push(createVisit({
    visitId: BigInt(0),
    normalizedUrl: {url: `${GOOGLE_SEARCH_BASE_URL}?q=foo`},
    urlForDisplay: 'www.google.com',
    pageTitle: 'SRP',
  }));

  // Create general visits.
  for (let i = 1; i <= numVisits; i++) {
    result.push(createVisit({
      visitId: BigInt(i),
      normalizedUrl: {url: `https://www.foo.com/${i}`},
      urlForDisplay: `www.foo.com/${i}`,
      pageTitle: `Test Title ${i}`,
      hasUrlKeyedImage: i <= numImageVisits,
    }));
  }
  return result;
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
