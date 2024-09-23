// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AutoTabGroupsPageElement, DeclutterPageElement, Tab, TabOrganizationSelectorButtonElement, TabOrganizationSelectorElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabOrganizationFeature, TabOrganizationState, TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTab, createTabOrganizationSession} from './tab_search_test_data.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabOrganizationSelectorTest', () => {
  let selector: TabOrganizationSelectorElement;
  let noSelectionState: HTMLElement;
  let autoTabGroupsState: AutoTabGroupsPageElement;
  let declutterState: DeclutterPageElement;
  let testApiProxy: TestTabSearchApiProxy;

  async function selectorSetup(staleTabCount: number = 3) {
    loadTimeData.overrideValues({
      declutterEnabled: true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    const staleTabs = createStaleTabs(staleTabCount);
    testApiProxy.setStaleTabs(staleTabs);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    selector = document.createElement('tab-organization-selector');
    document.body.appendChild(selector);
    await microtasksFinished();

    noSelectionState = selector.shadowRoot!.querySelector('#buttonContainer')!;
    assertTrue(!!noSelectionState);
    autoTabGroupsState =
        selector.shadowRoot!.querySelector('auto-tab-groups-page')!;
    assertTrue(!!autoTabGroupsState);
    declutterState = selector.shadowRoot!.querySelector('declutter-page')!;
    assertTrue(!!declutterState);
  }

  function createStaleTabs(count: number): Tab[] {
    const tabs: Tab[] = [];
    for (let i = 0; i < count; i++) {
      tabs.push(createTab({title: 'Tab', url: {url: 'https://tab.com/'}}));
    }
    return tabs;
  }

  test('Navigates to auto tab groups', async () => {
    await selectorSetup();
    assertTrue(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
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
    assertTrue(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
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

  test('Declutter navigates to selector', async () => {
    await selectorSetup();
    const declutterButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#declutterButton');
    assertTrue(!!declutterButton);
    declutterButton.click();
    await microtasksFinished();

    assertFalse(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
    assertTrue(isVisible(declutterState));

    const declutterBackButton =
        declutterState.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!declutterBackButton);
    declutterBackButton.click();
    await microtasksFinished();

    assertTrue(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
    assertFalse(isVisible(declutterState));
  });

  test('Auto tab groups base state navigates to selector', async () => {
    await selectorSetup();
    const autoTabGroupsButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#autoTabGroupsButton');
    assertTrue(!!autoTabGroupsButton);
    autoTabGroupsButton.click();
    await microtasksFinished();

    assertFalse(isVisible(noSelectionState));
    assertTrue(isVisible(autoTabGroupsState));
    assertFalse(isVisible(declutterState));

    const autoTabGroupsBackButton =
        autoTabGroupsState.shadowRoot!.querySelector<HTMLElement>(
            '.back-button');
    assertTrue(!!autoTabGroupsBackButton);
    autoTabGroupsBackButton.click();
    await microtasksFinished();

    assertTrue(isVisible(noSelectionState));
    assertFalse(isVisible(autoTabGroupsState));
    assertFalse(isVisible(declutterState));
  });

  test(
      'Auto tab groups non-base state doesnt navigate to selector',
      async () => {
        await selectorSetup();
        const session = createTabOrganizationSession({
          state: TabOrganizationState.kInProgress,
        });
        testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
            session);
        await microtasksFinished();

        const autoTabGroupsButton =
            selector.shadowRoot!.querySelector<HTMLElement>(
                '#autoTabGroupsButton');
        assertTrue(!!autoTabGroupsButton);
        autoTabGroupsButton.click();
        await microtasksFinished();

        assertFalse(isVisible(noSelectionState));
        assertTrue(isVisible(autoTabGroupsState));
        assertFalse(isVisible(declutterState));

        const autoTabGroupsBackButton =
            autoTabGroupsState.shadowRoot!.querySelector('cr-icon-button');
        assertTrue(!!autoTabGroupsBackButton);
        autoTabGroupsBackButton.click();
        await microtasksFinished();

        assertFalse(isVisible(noSelectionState));
        assertTrue(isVisible(autoTabGroupsState));
        assertFalse(isVisible(declutterState));
      });

  test('Disables declutter when no stale tabs', async () => {
    await selectorSetup(0);

    const declutterButton =
        selector.shadowRoot!
            .querySelector<TabOrganizationSelectorButtonElement>(
                '#declutterButton');
    assertTrue(!!declutterButton);
    assertTrue(declutterButton.disabled);

    testApiProxy.getCallbackRouterRemote().staleTabsChanged(createStaleTabs(3));
    await microtasksFinished();

    assertFalse(declutterButton.disabled);
  });

  test('Navigation calls setOrganizationFeature', async () => {
    await selectorSetup();
    assertEquals(0, testApiProxy.getCallCount('setOrganizationFeature'));

    const declutterButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#declutterButton');
    assertTrue(!!declutterButton);
    declutterButton.click();

    const [declutterFeature] =
        await testApiProxy.whenCalled('setOrganizationFeature');
    assertEquals(TabOrganizationFeature.kDeclutter, declutterFeature);
    testApiProxy.resetResolver('setOrganizationFeature');

    const declutterBackButton =
        declutterState.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!declutterBackButton);
    declutterBackButton.click();

    const [selectorFeature] =
        await testApiProxy.whenCalled('setOrganizationFeature');
    assertEquals(TabOrganizationFeature.kSelector, selectorFeature);
    testApiProxy.resetResolver('setOrganizationFeature');

    const autoTabGroupsButton =
        selector.shadowRoot!.querySelector<HTMLElement>('#autoTabGroupsButton');
    assertTrue(!!autoTabGroupsButton);
    autoTabGroupsButton.click();

    const [autoTabGroupsFeature] =
        await testApiProxy.whenCalled('setOrganizationFeature');
    assertEquals(TabOrganizationFeature.kAutoTabGroups, autoTabGroupsFeature);
  });
});
