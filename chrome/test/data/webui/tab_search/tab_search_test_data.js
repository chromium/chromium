// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function sampleData() {
  const profileTabs = {
    windows: [
      {
        active: true,
        tabs: [
          {
            index: 0,
            tabId: 1,
            title: 'Google',
            url: 'https://www.google.com',
          },
          {
            index: 1,
            tabId: 5,
            title: 'Amazon',
            url: 'https://www.amazon.com',
          },
          {
            index: 2,
            tabId: 6,
            title: 'Apple',
            url: 'https://www.apple.com',
          },
        ],
      },
      {
        active: false,
        tabs: [
          {
            index: 0,
            tabId: 2,
            title: 'Bing',
            url: 'https://www.bing.com/',
          },
          {
            index: 1,
            tabId: 3,
            title: 'Yahoo',
            url: 'https://www.yahoo.com',
          },
          {
            index: 2,
            tabId: 4,
            title: 'Apple',
            url: 'https://www.apple.com/',
          },
        ],
      }
    ]
  };

  return profileTabs;
}

/**
 * @return {!Array<string>}
 */
export function sampleSiteNames() {
  return [
    'Google',
    'Amazon',
    'Apple',
    'Bing',
    'Yahoo',
    'PayPal',
    'Square',
    'Youtube',
    'Facebook',
    'Twitter',
  ];
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
    windows: [{active: true, tabs: generateSampleTabsFromSiteNames(siteNames)}]
  };
}
