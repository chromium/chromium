// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';

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

  test('oneEntry', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';

    viewElement.addSelector('dummyPage1', dummyPage1);
    viewElement.addSelector('dummyPage2', dummyPage2);

    await waitAfterNextRender(viewElement);

    const sideNav = viewElement.shadowRoot.querySelector('navigation-selector');
    const navElements = sideNav.shadowRoot.querySelectorAll('.navigation-item');

    // Click the first menu item. Expect that the dummyPage1 to be created and
    // not hidden.
    navElements[0].click();
    await waitAfterNextRender(viewElement);
    const dummyElement1 =
        viewElement.shadowRoot.querySelector(`#${dummyPage1}`);
    assertFalse(dummyElement1.hidden);
    dummyElement1['onNavigationPageChanged'] = onNavigationPageChanged;

    // Click the second menu item. Expect that the dummyPage2 to be created and
    // not hidden. dummyPage1 should be hidden now.
    navElements[1].click();
    await waitAfterNextRender(viewElement);
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
    await waitAfterNextRender(viewElement);
    assertTrue(dummyElement2.hidden);
    assertFalse(dummyElement1.hidden);
    // Now that both dummy pages have implemented "onNavigationPageChanged",
    // the navigation click will trigger both page's methods.
    assertEquals(3, numPageChangedCount);
  });

  test('notifyEvent', async () => {
    const dummyPage1 = 'dummy-page1';

    viewElement.addSelector('dummyPage1', dummyPage1);

    await waitAfterNextRender(viewElement);

    const sideNav = viewElement.shadowRoot.querySelector('navigation-selector');
    const navElements = sideNav.shadowRoot.querySelectorAll('.navigation-item');

    // Create the element.
    navElements[0].click();
    await waitAfterNextRender(viewElement);
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

    viewElement.addSelector('dummyPage1', dummyPage1);
    viewElement.addSelector('dummyPage2', dummyPage2);

    assertFalse(viewElement.shadowRoot.querySelector(`#${dummyPage1}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${dummyPage2}`));
  });

  test('defaultCollapsiblePage', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';
    const subPage = 'sub-page1';

    let subItem =
        /** @type {SelectorItem} */ ({'name': 'subItem', 'pageIs': subPage});

    viewElement.addSelector('dummyPage1', dummyPage1, '', [subItem]);
    viewElement.addSelector('dummyPage2', dummyPage2);

    assertFalse(viewElement.shadowRoot.querySelector(`#${subPage}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${dummyPage2}`));
  });
}
