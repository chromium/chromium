// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Tab, TabOrganizationPageElement, TabOrganizationResultsElement, TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabOrganizationPageTest', () => {
  let tabOrganizationPage: TabOrganizationPageElement;
  let tabOrganizationResults: TabOrganizationResultsElement;
  let testProxy: TestTabSearchApiProxy;

  async function tabOrganizationPageSetup() {
    testProxy = new TestTabSearchApiProxy();
    TabSearchApiProxyImpl.setInstance(testProxy);

    tabOrganizationPage = document.createElement('tab-organization-page');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabOrganizationPage);
    await flushTasks();
  }

  async function tabOrganizationResultsSetup() {
    testProxy = new TestTabSearchApiProxy();
    TabSearchApiProxyImpl.setInstance(testProxy);

    tabOrganizationResults = document.createElement('tab-organization-results');
    tabOrganizationResults.name = 'Test name';
    tabOrganizationResults.tabs = [
      createTab({title: 'Tab 1', url: {url: 'https://tab-1.com/'}}),
      createTab({title: 'Tab 2', url: {url: 'https://tab-2.com/'}}),
      createTab({title: 'Tab 3', url: {url: 'https://tab-3.com/'}}),
    ];

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabOrganizationResults);
    await flushTasks();
  }

  function createTab(override: Partial<Tab> = {}): Tab {
    return Object.assign(
        {
          active: false,
          alertStates: [],
          index: -1,
          tabId: -1,
          groupId: -1,
          pinned: false,
          title: '',
          url: {url: 'about:blank'},
          isDefaultFavicon: false,
          showIcon: false,
          lastActiveTimeTicks: -1,
          lastActiveElapsedText: '',
        },
        override);
  }

  test('Organize tabs starts request', async () => {
    await tabOrganizationPageSetup();
    assertEquals(0, testProxy.getCallCount('requestTabOrganization'));
    const notStarted = tabOrganizationPage.shadowRoot!.querySelector(
        'tab-organization-not-started');
    assertTrue(!!notStarted);
    assertTrue(isVisible(notStarted));

    const organizeTabsButton =
        notStarted.shadowRoot!.querySelector('cr-button');
    assertTrue(!!organizeTabsButton);
    organizeTabsButton.click();

    assertEquals(1, testProxy.getCallCount('requestTabOrganization'));
    // TODO(emshack): Replace with check against in progress state once in
    // progress state exists as a separate component
    assertFalse(isVisible(notStarted));
  });

  test('Input blurs on enter', async () => {
    await tabOrganizationResultsSetup();
    const input = tabOrganizationResults.$.input;
    assertFalse(input.hasAttribute('focused_'));

    input.focus();
    assertTrue(input.hasAttribute('focused_'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertFalse(input.hasAttribute('focused_'));
  });

  test('Tab close removes from tab list', async () => {
    await tabOrganizationResultsSetup();

    const tabRows =
        tabOrganizationResults.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const cancelButton =
        tabRows[0]!.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await flushTasks();

    const tabRowsAfterCancel =
        tabOrganizationResults.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRowsAfterCancel);
    assertEquals(2, tabRowsAfterCancel.length);
  });
});
