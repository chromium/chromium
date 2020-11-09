// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertGE, assertLE} from '../../chai_assert.js';

/**
 * Override the scrollIntoView function and parameters of the given class to
 * avoid smooth scroll animations that delay the scrollTop property updates.
 * @param {!Object} klass
 */
export function disableScrollIntoViewAnimations(klass) {
  const originalScrollFunc = klass.prototype.scrollIntoView;
  klass.prototype.scrollIntoView = function(options) {
    const args = [];
    if (typeof options === 'object' && options !== null) {
      let noAnimationOptions = Object.assign({}, options);
      delete noAnimationOptions.behavior;

      args.push(noAnimationOptions);
    }
    originalScrollFunc.apply(this, args);
  };
}

/**
 * Assert that the tabItem HTML element is fully visible within the current
 * scroll view.
 * @param {!HTMLElement} tabsDiv
 * @param {!HTMLElement} tabItem
 */
export function assertTabItemInViewBounds(tabsDiv, tabItem) {
  assertGE(tabItem.offsetTop, tabsDiv.scrollTop);

  assertLE(
      tabItem.offsetTop + tabItem.offsetHeight,
      tabsDiv.scrollTop + tabsDiv.offsetHeight);
}

/**
 * @param {!HTMLElement} tabsDiv The HTML element containing a list of tab
 *     items.
 * @param {!NodeList<!HTMLElement>} tabItems A list of tab items.
 * @param {number} index The tab item's index in the list of tab items.
 */
export function assertTabItemAndNeighborsInViewBounds(
    tabsDiv, tabItems, index) {
  if (index > 0) {
    assertTabItemInViewBounds(tabsDiv, tabItems[index - 1]);
  }

  assertTabItemInViewBounds(tabsDiv, tabItems[index]);

  if (index < tabItems.length - 1) {
    assertTabItemInViewBounds(tabsDiv, tabItems[index + 1]);
  }
}

/**
 * Initialize the loadTimeData with the provided data and defaults.
 * @param {Object=} loadTimeOverriddenData
 */
export function initLoadTimeDataWithDefaults(loadTimeOverriddenData) {
  if (!loadTimeOverriddenData) {
    loadTimeOverriddenData = {};
  }
  if (!loadTimeOverriddenData.hasOwnProperty('shortcutText')) {
    loadTimeOverriddenData.shortcutText = '';
  }

  loadTimeData.overrideValues(loadTimeOverriddenData);
}
