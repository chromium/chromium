// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertGE, assertLE} from 'chrome://webui-test/chai_assert.js';

/**
 * Override the specified function and parameters for the given class to avoid
 * scroll animations that delay the scrollTop property updates.
 */
export function disableAnimationBehavior(klass: any, functionName: string) {
  const originalFunction = klass.prototype[functionName];
  klass.prototype[functionName] = function(options: any) {
    const args = [];
    if (typeof options === 'object' && options !== null) {
      const noAnimationOptions = Object.assign({}, options);
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
 * Initialize the loadTimeData with the provided data and defaults.
 */
export function initLoadTimeDataWithDefaults(
    loadTimeOverriddenData: {[key: string]: number|string|boolean} = {}) {
  loadTimeData.overrideValues(Object.assign(
      {
        shortcutText: '',
        recentlyClosedDefaultItemDisplayCount: 5,
      },
      loadTimeOverriddenData));
}

/**
 * Returns a style property number value that needs to be determined from the
 * computed style of an HTML element.
 */
export function getStylePropertyPixelValue(
    element: HTMLElement, name: string): number {
  const pxValue = getComputedStyle(element).getPropertyValue(name);
  return Number.parseInt(pxValue.trim().slice(0, -2), 10);
}
