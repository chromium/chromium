// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {ProfileData} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertGE, assertLE} from 'chrome://webui-test/chai_assert.js';

/**
 * Override the specified function and parameters for the given class to avoid
 * scroll animations that delay the scrollTop property updates.
 * @param {!Object} klass
 */
export function disableAnimationBehavior(klass: any, functionName: string) {
  const originalFunction = klass.prototype[functionName];
  klass.prototype[functionName] = function(options: any) {
    const args = [];
    if (typeof options === 'object' && options !== null) {
      let noAnimationOptions = Object.assign({}, options);
      delete noAnimationOptions.behavior;

      args.push(noAnimationOptions);
    }
    originalFunction.apply(this, args);
  };
}

/**
 * Assert that the tabItem HTML element is fully visible within the current
 * scroll view.
 */
export function assertTabItemInViewBounds(
    tabsDiv: HTMLElement, tabItem: HTMLElement) {
  assertGE(tabItem.offsetTop, tabsDiv.scrollTop);

  assertLE(
      tabItem.offsetTop + tabItem.offsetHeight,
      tabsDiv.scrollTop + tabsDiv.offsetHeight);
}

/**
 * @param tabsDiv The HTML element containing a list of tab items.
 * @param tabItems A list of tab items.
 * @param index The tab item's index in the list of tab items.
 */
export function assertTabItemAndNeighborsInViewBounds(
    tabsDiv: HTMLElement, tabItems: NodeListOf<HTMLElement>, index: number) {
  if (index > 0) {
    assertTabItemInViewBounds(tabsDiv, tabItems[index - 1]!);
  }

  assertTabItemInViewBounds(tabsDiv, tabItems[index]!);

  if (index < tabItems.length - 1) {
    assertTabItemInViewBounds(tabsDiv, tabItems[index + 1]!);
  }
}

/**
 * Initialize a mock ProfileData object with defaults that would be set
 * by the Mojo IPC logic.
 */
export function initProfileDataWithDefaults(profileData: ProfileData) {
  ['tabGroups', 'recentlyClosedTabs', 'recentlyClosedTabGroups'].forEach(
      (arrayProp) => {
        if (!profileData.hasOwnProperty(arrayProp)) {
          (profileData as {[key: string]: any})[arrayProp] = [];
        }
      });
  if (!profileData.hasOwnProperty('recentlyClosedSectionExpanded')) {
    profileData.recentlyClosedSectionExpanded = false;
  }
}

/**
 * Initialize the loadTimeData with the provided data and defaults.
 */
export function initLoadTimeDataWithDefaults(
    loadTimeOverriddenData: {[key: string]: string} = {}) {
  loadTimeData.overrideValues(Object.assign(
      {
        shortcutText: '',
        recentlyClosedDefaultItemDisplayCount: 5,
      },
      loadTimeOverriddenData));
}
