// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';

export function navigationViewPanelTestSuite() {
  /** @type {?NavigationViewPanelElement} */
  let viewElement = null;

  setup(() => {
    viewElement =
        /** @type {!NavigationViewPanelElement} */ (
            document.createElement('navigation-view-panel'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement.remove();
    viewElement = null;
  });

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

    // Click the second menu item. Expect that the dummyPage2 to be created and
    // not hidden. dummyPage1 should be hidden now.
    navElements[1].click();
    await waitAfterNextRender(viewElement);
    const dummyElement2 =
        viewElement.shadowRoot.querySelector(`#${dummyPage2}`);
    assertFalse(dummyElement2.hidden);
    assertTrue(dummyElement1.hidden);

    // Click the first menu item. Expect that dummyPage2 is now hidden and
    // dummyPage1 is not hidden.
    navElements[0].click();
    await waitAfterNextRender(viewElement);
    assertTrue(dummyElement2.hidden);
    assertFalse(dummyElement1.hidden);
  });
}
