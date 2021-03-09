// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const SAMPLE_WINDOW_HEIGHT = 448;

export function sampleData() {
  return {
    windows: [
      {
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [
          {
            index: 0,
            tabId: 1,
            title: 'Google',
            url: 'https://www.google.com',
            lastActiveTimeTicks: {internalValue: BigInt(5)},
          },
          {
            index: 1,
            tabId: 5,
            title: 'Amazon',
            url: 'https://www.amazon.com',
            lastActiveTimeTicks: {internalValue: BigInt(4)},
          },
          {
            index: 2,
            tabId: 6,
            title: 'Apple',
            url: 'https://www.apple.com',
            lastActiveTimeTicks: {internalValue: BigInt(3)},
          },
        ],
      },
      {
        active: false,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [
          {
            index: 0,
            tabId: 2,
            title: 'Bing',
            url: 'https://www.bing.com/',
            lastActiveTimeTicks: {internalValue: BigInt(2)},
          },
          {
            index: 1,
            tabId: 3,
            title: 'Yahoo',
            url: 'https://www.yahoo.com',
            lastActiveTimeTicks: {internalValue: BigInt(1)},
          },
          {
            index: 2,
            tabId: 4,
            title: 'Apple',
            url: 'https://www.apple.com/',
            lastActiveTimeTicks: {internalValue: BigInt(0)},
          },
        ],
      }
    ]
  };
}

/**
 * @param count
 * @return {!Array<string>}
 */
export function sampleSiteNames(count) {
  return Array.from({length: count}, (_, i) => (i + 1).toString());
}

/**
 * Generates sample tabs based on some given site names.
 * @param {!Array} siteNames
 * @return {!Array}
 */
export function generateSampleTabsFromSiteNames(siteNames) {
  return siteNames.map((siteName, i) => {
    return {
      index: i,
      tabId: i + 1,
      title: siteName,
      url: 'https://www.' + siteName.toLowerCase() + '.com',
      lastActiveTimeTicks: siteNames.length - i,
    };
  });
}

/**
 * Generates profile data for a window with a series of tabs.
 * @param {!Array} siteNames
 * @return {!Object}
 */
export function generateSampleDataFromSiteNames(siteNames) {
  return {
    windows: [{
      active: true,
      height: SAMPLE_WINDOW_HEIGHT,
      tabs: generateSampleTabsFromSiteNames(siteNames)
    }]
  };
}
