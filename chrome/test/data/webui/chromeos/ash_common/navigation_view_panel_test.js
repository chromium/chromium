// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function navigationViewPanelTestSuite() {
  /** @type {?NavigationViewPanelElement} */
  let viewElement = null;

  /** @type {number} */
  let eventCount = 0;
  /** @type {!Object} */
  let eventDetail = {};
  /** @type {number} */
  let numPageChangedCount = 0;

  setup(() => {
    viewElement =
        /** @type {!NavigationViewPanelElement} */ (
            document.createElement('navigation-view-panel'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement.remove();
    viewElement = null;
    eventCount = 0;
    eventDetail = {};
    numPageChangedCount = 0;
  });

  /**
   * @param {!Object} e
   */
  function handleEvent(e) {
    eventDetail = e;
    eventCount++;
  }

  /**
   * @param {!CustomEvent} e
   */
  function onNavigationPageChanged(e) {
    numPageChangedCount++;
  }

  /**
   * @return {!NodeList<!HTMLElement>}
   */
  function getNavElements() {
    const sideNav = viewElement.shadowRoot.querySelector('navigation-selector');
    assert(!!sideNav);
    const navElements = sideNav.shadowRoot.querySelectorAll('.navigation-item');
    assert(!!navElements);
    return navElements;
  }

  /**
   * Adds a section to the navigation element.
   * @param {string} name
   * @param {string} pageType
   * @param {string} icon
   * @param {?string} id
   * @param {!Array<SelectorItem>} subItems
   * @return {!Promise}
   */
  function addNavigationSection(
      name, pageType, icon = '', id = null, subItems = []) {
    viewElement.addSelector(name, pageType, icon, id, subItems);
    return flushTasks();
  }

  test('twoEntries', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';
    await addNavigationSection('dummyPage1', dummyPage1);
    await addNavigationSection('dummyPage2', dummyPage2);

    // Click the first menu item. Expect that the dummyPage1 to be created and
    // not hidden.
    const navElements = getNavElements();
    navElements[0].click();
    await flushTasks();
    const dummyElement1 =
        viewElement.shadowRoot.querySelector(`#${dummyPage1}`);
    assertFalse(dummyElement1.hidden);
    dummyElement1['onNavigationPageChanged'] = onNavigationPageChanged;

    // Click the second menu item. Expect that the dummyPage2 to be created and
    // not hidden. dummyPage1 should be hidden now.
    navElements[1].click();
    await flushTasks();
    const dummyElement2 =
        viewElement.shadowRoot.querySelector(`#${dummyPage2}`);
    dummyElement2['onNavigationPageChanged'] = onNavigationPageChanged;
    assertFalse(dummyElement2.hidden);
    assertTrue(dummyElement1.hidden);
    // Only one page has implemented "onNavigationPageChanged" by the second
    // navigation click, expect only one client to be notified.
    assertEquals(1, numPageChangedCount);

    // Click the first menu item. Expect that dummyPage2 is now hidden and
    // dummyPage1 is not hidden.
    navElements[0].click();
    await flushTasks();
    assertTrue(dummyElement2.hidden);
    assertFalse(dummyElement1.hidden);
    // Now that both dummy pages have implemented "onNavigationPageChanged",
    // the navigation click will trigger both page's methods.
    assertEquals(3, numPageChangedCount);
  });

  test('notifyEvent', async () => {
    const dummyPage1 = 'dummy-page1';
    await addNavigationSection('dummyPage1', dummyPage1);

    // Create the element.
    const navElements = getNavElements();
    navElements[0].click();
    await flushTasks();
    const dummyElement = viewElement.shadowRoot.querySelector(`#${dummyPage1}`);

    const functionName = 'onEventReceived';
    const expectedDetail = 'test';
    // Set the function handler for the element.
    dummyElement[functionName] = handleEvent;
    // Trigger notifyEvent and expect |dummyElement| to capture the event.
    viewElement.notifyEvent(functionName, {detail: expectedDetail});

    assertEquals(1, eventCount);
    assertEquals(expectedDetail, eventDetail.detail);
  });

  test('defaultPage', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';

    await addNavigationSection('dummyPage1', dummyPage1);
    await addNavigationSection('dummyPage2', dummyPage2);

    assertFalse(viewElement.shadowRoot.querySelector(`#${dummyPage1}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${dummyPage2}`));
  });

  test('samePageTypeDifferentId', async () => {
    const pageType = 'myPageType';
    const id1 = 'id1';
    const id2 = 'id2';

    // Add two pages of the the same type with different ids.
    await addNavigationSection('Page 1', pageType, /*icon=*/ '', 'id1');
    await addNavigationSection('Page 2', pageType, /*icon=*/ '', 'id2');

    // First page should be created by default.
    assertTrue(!!viewElement.shadowRoot.querySelector(`#${id1}`));
    assertFalse(viewElement.shadowRoot.querySelector(`#${id1}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${id2}`));

    // Nav to the second page and it should be created and the first page
    // should be hidden.
    const navElements = getNavElements();
    navElements[1].click();
    await flushTasks();

    assertTrue(viewElement.shadowRoot.querySelector(`#${id1}`).hidden);
    assertTrue(!!viewElement.shadowRoot.querySelector(`#${id2}`));
    assertFalse(viewElement.shadowRoot.querySelector(`#${id2}`).hidden);
  });

  test('defaultCollapsiblePage', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';
    const subPage = 'sub-page1';
    const subid = 'subid1';
    const id1 = 'id1';

    let subItem =
        /** @type {SelectorItem} */ (
            {'name': 'subItem', 'pageIs': subPage, 'id': subid});

    await addNavigationSection('dummyPage1', dummyPage1, '', id1, [subItem]);
    await addNavigationSection('dummyPage2', dummyPage2);

    // The pages are not created yet.
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${id1}`));
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${dummyPage2}`));
  });
}
