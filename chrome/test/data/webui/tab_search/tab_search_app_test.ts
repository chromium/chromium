// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {TabSearchApiProxyImpl, TabSearchAppElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabOrganizationPageTest', () => {
  let tabSearchApp: TabSearchAppElement;
  let testProxy: TestTabSearchApiProxy;

  setup(async () => {
    testProxy = new TestTabSearchApiProxy();
    TabSearchApiProxyImpl.setInstance(testProxy);

    loadTimeData.overrideValues({
      tabOrganizationEnabled: true,
    });

    tabSearchApp = document.createElement('tab-search-app');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabSearchApp);
    await flushTasks();
  });

  test('Switching tabs calls setTabIndex', async () => {
    assertEquals(0, testProxy.getCallCount('setTabIndex'));

    const crTabs = tabSearchApp.shadowRoot!.querySelector('cr-tabs');
    assertTrue(!!crTabs);
    assertEquals(0, crTabs.selected);

    const allTabs = crTabs.shadowRoot!.querySelectorAll<HTMLElement>('.tab');
    assertEquals(2, allTabs.length);
    const newTabIndex = 1;
    const unselectedTab = allTabs[newTabIndex]!;
    unselectedTab.click();

    const [tabIndex] = await testProxy.whenCalled('setTabIndex');
    assertEquals(newTabIndex, tabIndex);
    assertEquals(newTabIndex, crTabs.selected);
  });
});
