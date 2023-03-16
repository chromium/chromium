// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchQuery, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS, MIN_RELATED_SEARCHES} from 'chrome://new-tab-page/lazy_load.js';

export function createVisit(
    visitId: bigint, normalizedUrl: string, urlForDisplay: string,
    pageTitle: string, hasUrlKeyedImage: boolean,
    relativeDate: string = ''): URLVisit {
  return {
    visitId: visitId,
    normalizedUrl: {url: normalizedUrl},
    urlForDisplay: urlForDisplay,
    pageTitle: pageTitle,
    titleMatchPositions: [],
    urlForDisplayMatchPositions: [],
    duplicates: [],
    relativeDate: relativeDate,
    annotations: [],
    debugInfo: {},
    rawVisitData: {
      url: {url: ''},
      visitTime: {internalValue: BigInt(0)},
    },
    hasUrlKeyedImage: hasUrlKeyedImage,
    isKnownToSync: false,
  };
}

// Use Layout 1 as default for tests that do not care which layout.
export function createSampleVisits(
    numVisits: number = LAYOUT_1_MIN_VISITS,
    numImageVisits: number = LAYOUT_1_MIN_IMAGE_VISITS): URLVisit[] {
  const result: URLVisit[] = [];

  // Create SRP visit.
  result.push(createVisit(
      BigInt(0), 'https://www.google.com/', 'www.google.com', 'SRP', false));

  // Create general visits.
  for (let i = 1; i <= numVisits; i++) {
    result.push(createVisit(
        BigInt(i), `https://www.foo.com/${i}`, `www.foo.com/${i}`,
        `Test Title ${i}`, i <= numImageVisits));
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
        url:
            `https://www.google.com/search?q=${encodeURIComponent(`test${i}`)}`,
      },
    });
  }
  return result;
}
