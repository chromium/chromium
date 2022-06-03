// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import {RecentlyClosedTab, Tab} from 'chrome://tab-search.top-chrome/tab_search.js';

/** @type {number} */
export const SAMPLE_WINDOW_HEIGHT = 448;

/** @type {!Array} */
export const SAMPLE_WINDOW_DATA = [
  {
    active: true,
    height: SAMPLE_WINDOW_HEIGHT,
    tabs: [
      {
        index: 0,
        tabId: 1,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(5)},
        lastActiveElapsedText: '',
      },
      {
        index: 1,
        tabId: 5,
        title: 'Amazon',
        url: {url: 'https://www.amazon.com'},
        lastActiveTimeTicks: {internalValue: BigInt(4)},
        lastActiveElapsedText: '',
      },
      {
        index: 2,
        tabId: 6,
        title: 'Apple',
        url: {url: 'https://www.apple.com'},
        lastActiveTimeTicks: {internalValue: BigInt(3)},
        lastActiveElapsedText: '',
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
        url: {url: 'https://www.bing.com/'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
        lastActiveElapsedText: '',
      },
      {
        index: 1,
        tabId: 3,
        title: 'Yahoo',
        url: {url: 'https://www.yahoo.com'},
        lastActiveTimeTicks: {internalValue: BigInt(1)},
        lastActiveElapsedText: '',
      },
      {
        index: 2,
        tabId: 4,
        title: 'Apple',
        url: {url: 'https://www.apple.com/'},
        lastActiveTimeTicks: {internalValue: BigInt(0)},
        lastActiveElapsedText: '',
      },
    ],
  }
];

/** @type {!Array<RecentlyClosedTab>} */
export const SAMPLE_RECENTLY_CLOSED_DATA = [
  {
    tabId: 100,
    title: 'PayPal',
    url: {url: 'https://www.paypal.com'},
    lastActiveTime: {internalValue: BigInt(11)},
    lastActiveElapsedText: '',
  },
  {
    tabId: 101,
    title: 'Stripe',
    url: {url: 'https://www.stripe.com'},
    lastActiveTime: {internalValue: BigInt(12)},
    lastActiveElapsedText: '',
  },
];

/** @return {!Array} */
export function sampleData() {
  return {
    windows: SAMPLE_WINDOW_DATA,
    recentlyClosedTabs: [],
    tabGroups: [],
    recentlyClosedTabGroups: [],
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
 * @param {!Array<string>} siteNames
 * @param {boolean} hasIndex Whether the items have an index property.
 * @return {!Array<!Tab>}
 */
export function generateSampleTabsFromSiteNames(siteNames, hasIndex = true) {
  return siteNames.map((siteName, i) => {
    const tab = /** @type {!Tab} */ ({
      tabId: i + 1,
      title: siteName,
      url: {url: 'https://www.' + siteName.toLowerCase() + '.com'},
      lastActiveTimeTicks: {internalValue: BigInt(siteNames.length - i)},
      lastActiveElapsedText: '',
    });

    if (hasIndex) {
      tab.index = i;
    }

    return tab;
  });
}

/**
 * @param {string} titlePrefix
 * @param {number} count
 * @param {Token} groupId
 * @return {!Array<!RecentlyClosedTab>}
 */
export function generateSampleRecentlyClosedTabs(
    titlePrefix, count, groupId = undefined) {
  return Array.from({length: count}, (_, i) => {
    const tabId = i + 1;
    const tab = /** @type {RecentlyClosedTab} */ ({
      tabId,
      title: `${titlePrefix} ${tabId}`,
      url: {url: `https://www.sampletab.com?q=${tabId}`},
      lastActiveTime: {internalValue: BigInt(count - i)},
      lastActiveElapsedText: '',
    });

    if (groupId !== undefined) {
      tab.groupId = groupId;
    }

    return tab;
  });
}

/**
 * Generates profile data for a window with a series of tabs.
 * @param {!Array<string>} siteNames
 * @return {!Object}
 */
export function generateSampleDataFromSiteNames(siteNames) {
  return {
    windows: [{
      active: true,
      height: SAMPLE_WINDOW_HEIGHT,
      tabs: generateSampleTabsFromSiteNames(siteNames)
    }],
    recentlyClosedTabs: [],
    tabGroups: [],
    recentlyClosedTabGroups: [],
  };
}

/**
 * @param {!bigint} high
 * @param {!bigint} low
 * @return {!Token}
 */
export function sampleToken(high, low) {
  const token = new Token();
  token.high = high;
  token.low = low;
  Object.freeze(token);

  return token;
}
