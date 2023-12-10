// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationSelectorElement, SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('navigationSelectorTestSuite', () => {
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
    const item = /** @type{SelectorItem} */ (
        {'name': name, 'pageIs': pageIs, 'icon': icon});
    return item;
  }

  test('navigationSelectorLoadEntries', async () => {
    const item1 = createSelectorItem('test1', 'test-page1', '');
    const item2 = createSelectorItem('test2', 'test-page2', '');

    const entries =
        /** @type{!Array<!SelectorItem>} */ ([item1, item2]);
    navigationElement.selectorItems = entries;

    await waitAfterNextRender(navigationElement);

    const navigationElements =
        navigationElement.shadowRoot.querySelectorAll('.navigation-item');
    assertEquals(2, navigationElements.length);
    assertEquals('test1', navigationElements[0].textContent.trim());
    assertEquals('test2', navigationElements[1].textContent.trim());
  });

  test('navigationSelectorIconVisible', async () => {
    const item1 = createSelectorItem('test1', 'test-page1', 'search');

    navigationElement.selectorItems = [item1];
    await waitAfterNextRender(navigationElement);

    const selectorElement =
        navigationElement.shadowRoot.querySelector('.navigation-item');
    assertTrue(!!selectorElement);

    const iconElement = selectorElement.querySelector('iron-icon');
    assertTrue(isVisible(iconElement));
  });

  test('navigationSelectorIconHidden', async () => {
    const item1 = createSelectorItem('test1', 'test-page1', '');

    navigationElement.selectorItems = [item1];
    await waitAfterNextRender(navigationElement);

    const selectorElement =
        navigationElement.shadowRoot.querySelector('.navigation-item');
    assertTrue(!!selectorElement);

    const iconElement = selectorElement.querySelector('iron-icon');
    assertFalse(isVisible(iconElement));
  });
});
