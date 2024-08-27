// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabOrganizationSelectorElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabOrganizationSelectorTest', () => {
  let selector: TabOrganizationSelectorElement;
  let testApiProxy: TestTabSearchApiProxy;

  function selectorSetup() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    selector = document.createElement('tab-organization-selector');
    document.body.appendChild(selector);
    return microtasksFinished();
  }

  test('Navigates to auto tab groups', async () => {
    await selectorSetup();
    const noSelectionState =
        selector.shadowRoot!.querySelector('#buttonContainer');
    assertTrue(!!noSelectionState);
    assertTrue(isVisible(noSelectionState));
    const autoTabGroupsState =
        selector.shadowRoot!.querySelector('auto-tab-groups-page');
    assertTrue(!!autoTabGroupsState);
    assertFalse(isVisible(autoTabGroupsState));
    const declutterState = selector.shadowRoot!.querySelector('declutter-page');
    assertTrue(!!declutterState);
    assertFalse(isVisible(declutterState));

    const autoTabGroupsButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#autoTabGroupsButton');
    assertTrue(!!autoTabGroupsButton);
    autoTabGroupsButton.click();
    await microtasksFinished();

    assertFalse(isVisible(noSelectionState));
    assertTrue(isVisible(autoTabGroupsState));
    assertFalse(isVisible(declutterState));
  });

  test('Navigates to declutter', async () => {
    await selectorSetup();
    const noSelectionState =
        selector.shadowRoot!.querySelector('#buttonContainer');
    assertTrue(!!noSelectionState);
    assertTrue(isVisible(noSelectionState));
    const autoTabGroupsState =
        selector.shadowRoot!.querySelector('auto-tab-groups-page');
    assertTrue(!!autoTabGroupsState);
    assertFalse(isVisible(autoTabGroupsState));
    const declutterState = selector.shadowRoot!.querySelector('declutter-page');
    assertTrue(!!declutterState);
    assertFalse(isVisible(declutterState));

    const declutterButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#declutterButton');
    assertTrue(!!declutterButton);
    declutterButton.click();
    await microtasksFinished();

    assertFalse(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
    assertTrue(isVisible(declutterState));
  });
});
