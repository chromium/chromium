// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSelectorItem, NavigationSelectorElement, SelectorItem, SelectorProperties} from 'chrome://resources/ash/common/navigation_selector.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';

export function navigationSelectorTestSuite() {
  /** @type {?NavigationSelectorElement} */
  let navigationElement = null;

  setup(() => {
    navigationElement =
        /** @type {!NavigationSelectorElement} */ (
            document.createElement('navigation-selector'));
    document.body.appendChild(navigationElement);
  });

  teardown(() => {
    navigationElement.remove();
    navigationElement = null;
  });

  /**
   * @param {string} name
   * @param {string} pageIs
   * @param {string} icon
   * @return {!SelectorItem}
   */
  function createSelectorItem(name, pageIs, icon) {
    let item = /** @type{SelectorItem} */ (
        {'name': name, 'pageIs': pageIs, 'icon': icon});
    return item;
  }

  /**
   * @param {boolean} isCollapsible
   * @param {boolean} isExpanded
   * @param {!Array<?SelectorItem>} subMenuItems
   * @return {!SelectorProperties}
   */
  function createProperty(isCollapsible, isExpanded, subMenuItems) {
    let property = /** @type{SelectorProperties} */ ({
      'isCollapsible': isCollapsible,
      'isExpanded': isExpanded,
      'subMenuItems': subMenuItems,
    });
    return property;
  }

  /**
   * @param {SelectorItem} selectorItem
   * @param {SelectorProperties} properties
   * @return {!MenuSelectorItem}
   */
  function createMenuItem(selectorItem, properties) {
    let menuItem = /** @type{!MenuSelectorItem} */ ({
      'selectorItem': selectorItem,
      'properties': properties,
    });
    return menuItem;
  }

  test('navigationSelectorLoadEntries', async () => {
    const item1 = createSelectorItem('test1', 'test-page1', '');
    const item2 = createSelectorItem('test2', 'test-page2', '');

    const property1 = createProperty(false, false, []);
    const property2 = createProperty(false, false, []);

    const menuItem1 = createMenuItem(item1, property1);
    const menuItem2 = createMenuItem(item2, property2);

    const entries =
        /** @type{!Array<!MenuSelectorItem>} */ ([menuItem1, menuItem2]);
    navigationElement.menuItems = entries;

    await waitAfterNextRender(navigationElement);

    const navigationElements =
        navigationElement.shadowRoot.querySelectorAll('.navigation-item');
    assertEquals(2, navigationElements.length);
    assertEquals('test1', navigationElements[0].textContent.trim());
    assertEquals('test2', navigationElements[1].textContent.trim());
  });

  test('navigationSelectorLoadsCollapsibleEntries', async () => {
    const item1 = createSelectorItem(
        'test1',
        'test-page1',
        '',
    );
    const item2 = createSelectorItem('Advanced', '', '');

    const property = createProperty(true, false, [item1]);

    const menuItem = createMenuItem(item2, property);

    const entries =
        /** @type{!Array<!MenuSelectorItem>} */ ([menuItem]);
    navigationElement.menuItems = entries;

    await waitAfterNextRender(navigationElement);

    const button = navigationElement.shadowRoot.querySelector('.expand-button');
    const ironCollapse =
        navigationElement.shadowRoot.querySelector('iron-collapse');

    assertTrue(!!button);
    assertFalse(ironCollapse.opened);
    assertEquals('Advanced', button.textContent.trim());

    // Click on the expandable entry.
    button.click();

    let navigationElements =
        navigationElement.shadowRoot.querySelectorAll('.navigation-item');
    assertEquals(1, navigationElements.length);
    assertEquals('test1', navigationElements[0].textContent.trim());
    assertTrue(ironCollapse.opened);
  });
}