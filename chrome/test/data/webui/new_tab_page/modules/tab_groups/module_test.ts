// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconContainerElement, TabGroupsModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX, COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX, colorIdToString, NTPPluralStringProxyImpl, tabGroupsDescriptor, TabGroupsProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {Color} from 'chrome://new-tab-page/tab_group_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import type {TabGroup} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesTabGroupsModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    handler = installMock(
        PageHandlerRemote,
        mock => TabGroupsProxyImpl.setInstance(new TabGroupsProxyImpl(mock)));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
  });

  async function createModule(
      tabGroups: TabGroup[]|null,
      showZeroState: boolean = false): Promise<TabGroupsModuleElement> {
    handler.setResultFor(
        'getTabGroups', Promise.resolve({tabGroups, showZeroState}));
    const module =
        await tabGroupsDescriptor.initialize(0) as TabGroupsModuleElement;
    document.body.append(module);
    await microtasksFinished();
    return module;
  }

  test('create module', async () => {
    // Arrange.
    const tabGroups: TabGroup[] = [
      {
        id: '0',
        color: Color.kBlue,
        title: 'Tab Group 1',
        updateTime: 'Recently used',
        deviceName: 'Test Device',
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 4,
        isSharedTabGroup: true,
      },
      {
        id: '0',
        color: Color.kGrey,
        title: 'Tab Group 2',
        updateTime: 'Recently used',
        deviceName: 'Test Device',
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 8,
        isSharedTabGroup: false,
      },
      {
        id: '0',
        color: Color.kRed,
        title: 'Tab Group 3',
        updateTime: 'Recently used',
        deviceName: null,
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 188,
        isSharedTabGroup: true,
      },
    ];
    const module = await createModule(tabGroups);

    // Assert.
    // Verify the module was created and is visible.
    assertTrue(!!module);
    assertTrue(isVisible(module.shadowRoot.querySelector('ntp-module-header')));

    // Verify the tab groups info is correct.
    const groups =
        module.shadowRoot.querySelectorAll<HTMLAnchorElement>('.tab-group');
    assertTrue(!!groups);
    assertEquals(tabGroups.length, groups.length);

    for (let i = 0; i < groups.length; ++i) {
      assertTrue(
          groups[i]!.querySelector('.color-dot')!.getAttribute('style')!
              .includes(`background-color: var(${
                  colorIdToString(
                      COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX,
                      tabGroups[i]!.color)})`));
      assertEquals(
          `Tab Group ${i + 1}`,
          groups[i]!.querySelector('.tab-group-title')!.textContent);
      const iconContainer =
          groups[i]!.querySelector<IconContainerElement>('ntp-icon-container')!;
      assertTrue(!!iconContainer);
      assertDeepEquals(
          tabGroups[i]!.faviconUrls.map(u => u.url), iconContainer.faviconUrls);
      assertEquals(tabGroups[i]!.totalTabCount, iconContainer.totalTabCount);
      assertTrue(iconContainer.getAttribute('style')!.includes(
          `background-color: var(${
              colorIdToString(
                  COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX,
                  tabGroups[i]!.color)})`));
      const sharedTabGroupIcon =
          groups[i]!.querySelector<CrIconElement>('#sharedTabGroupIcon');
      assertEquals(tabGroups[i]!.isSharedTabGroup, !!sharedTabGroupIcon);
    }

    // Verify that optional device info is parsed correclty.
    assertEquals(
        'Recently used • Test Device',
        groups[0]!.querySelector('.tab-group-description')!.textContent);
    assertEquals(
        'Recently used • Test Device',
        groups[1]!.querySelector('.tab-group-description')!.textContent);
    assertEquals(
        'Recently used',
        groups[2]!.querySelector('.tab-group-description')!.textContent);
  });

  test('module not created when getTabGroups returns null', async () => {
    const module = await createModule(null);
    assertFalse(!!module);
  });

  function getIconContainerElement(
      module: TabGroupsModuleElement, index: number): IconContainerElement {
    const groups =
        module.shadowRoot.querySelectorAll<HTMLAnchorElement>('.tab-group');
    return groups[index]!.querySelector<IconContainerElement>(
        'ntp-icon-container')!;
  }

  test('show empty cells if there are less than four tabs', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
      isSharedTabGroup: false,
    }]);

    // Assert.
    const iconContainer = getIconContainerElement(module, 0);
    const cells = iconContainer.shadowRoot.querySelectorAll('.cell');
    assertEquals(4, cells.length);

    const iconCells = iconContainer.shadowRoot.querySelectorAll('.cell.icon');
    const emptyCells = iconContainer.shadowRoot.querySelectorAll('.cell.empty');
    const overflowCells =
        iconContainer.shadowRoot.querySelectorAll('.cell.overflow-count');
    assertEquals(1, iconCells.length);
    assertEquals(3, emptyCells.length);
    assertEquals(0, overflowCells.length);
  });

  test('show four favicons when there are exactly four tabs', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 4,
      isSharedTabGroup: false,
    }]);

    // Assert.
    const iconContainer = getIconContainerElement(module, 0);
    const cells = iconContainer.shadowRoot.querySelectorAll('.cell');
    assertEquals(4, cells.length);

    const iconCells = iconContainer.shadowRoot.querySelectorAll('.cell.icon');
    const overflowCells =
        iconContainer.shadowRoot.querySelectorAll('.cell.overflow-count');
    assertEquals(4, iconCells.length);
    assertEquals(0, overflowCells.length);
  });

  test('show +N when more than one tab remains', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 8,
      isSharedTabGroup: false,
    }]);

    // Assert.
    const iconContainer = getIconContainerElement(module, 0);
    const cells = iconContainer.shadowRoot.querySelectorAll('.cell');
    assertEquals(4, cells.length);

    const iconCells = iconContainer.shadowRoot.querySelectorAll('.cell.icon');
    const overflowCells =
        iconContainer.shadowRoot.querySelectorAll('.cell.overflow-count');
    assertEquals(3, iconCells.length);
    assertEquals(1, overflowCells.length);

    const overflowText = overflowCells[0]!.textContent.trim();
    assertEquals('+5', overflowText);
  });

  test('caps at 99+ when more than 99 tabs remain', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 188,
      isSharedTabGroup: false,
    }]);

    // Assert.
    const iconContainer = getIconContainerElement(module, 0);
    const cells = iconContainer.shadowRoot.querySelectorAll('.cell');
    assertEquals(4, cells.length);

    const iconCells = iconContainer.shadowRoot.querySelectorAll('.cell.icon');
    const overflowCells =
        iconContainer.shadowRoot.querySelectorAll('.cell.overflow-count');
    assertEquals(3, iconCells.length);
    assertEquals(1, overflowCells.length);

    const overflowText = overflowCells[0]!.textContent.trim();
    assertEquals('99+', overflowText);
  });

  test('action menu - open and close info dialog', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
      isSharedTabGroup: false,
    }]);

    // Assert.
    // Open the dialog.
    assertTrue(!!module);
    const headerElement = module.shadowRoot.querySelector('ntp-module-header');
    assertTrue(!!headerElement);
    const infoButton =
        headerElement.shadowRoot.querySelector<HTMLButtonElement>('#info');
    assertTrue(!!infoButton);
    infoButton.click();
    await microtasksFinished();
    const dialog = module.shadowRoot.querySelector('ntp-info-dialog');
    assertTrue(!!dialog);

    // Validate dialog text.
    const text = dialog.textContent.replace(/\s+/g, ' ').trim();
    assertTrue(text.includes(
        'You’re seeing your recently used tab groups to help you easily pick up where you left off.'));
    assertTrue(text.includes(
        'You can manage settings from the card menu or see more options in Customize Chrome.'));

    // Close the dialog.
    dialog.dispatchEvent(new CustomEvent('close'));
    await microtasksFinished();
    assertFalse(!!module.shadowRoot.querySelector('ntp-info-dialog'));
  });


  test('action menu - disable button fires disable toast', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
      isSharedTabGroup: false,
    }]);
    assertTrue(!!module);

    // Act.
    const whenFired = eventToPromise('disable-module', module);
    const headerElement = module.shadowRoot.querySelector('ntp-module-header');
    assertTrue(!!headerElement);
    headerElement.dispatchEvent(new Event('disable-button-click'));

    // Assert.
    const {detail} = await whenFired;
    assertEquals(
        loadTimeData.getString('modulesTabGroupsDisableToastMessage'),
        detail.message);
  });

  test('action menu - dismiss and restore module', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Tab Group',
      updateTime: 'Recently used',
      deviceName: 'Test Device',
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
      isSharedTabGroup: false,
    }]);
    assertTrue(!!module);

    // Act.
    const whenFired = eventToPromise('dismiss-module-instance', module);
    const headerElement = module.shadowRoot.querySelector('ntp-module-header');
    assertTrue(!!headerElement);
    headerElement.dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    const {detail} = await whenFired;
    assertEquals(
        loadTimeData.getString('modulesTabGroupsDismissToastMessage'),
        detail.message);
    assertTrue(!!detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    detail.restoreCallback();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('create new tab group from the footer button', async () => {
    // Arrange.
    const module = await createModule([{
      id: '0',
      color: Color.kBlue,
      title: 'Group',
      updateTime: 'Recently used',
      deviceName: null,
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
      isSharedTabGroup: false,
    }]);
    assertTrue(!!module);

    const createNewTabGroupButton =
        module.shadowRoot.querySelector<CrButtonElement>(
            '#createNewTabGroupFooterButton');
    assertTrue(!!createNewTabGroupButton);
    assertTrue(isVisible(createNewTabGroupButton));

    // Act.
    handler.setResultFor('createNewTabGroup', Promise.resolve());
    createNewTabGroupButton.click();
    await microtasksFinished();

    // Assert.
    assertEquals(1, handler.getCallCount('createNewTabGroup'));
    assertEquals(1, metrics.count('NewTabPage.TabGroups.CreateNewTabGroup'));
    assertEquals(
        1, metrics.count('NewTabPage.TabGroups.CreateNewTabGroup.SteadyState'));
  });

  test('open a tab group and fire openTabGroup with the group ID', async () => {
    // Arrange.
    const module = await createModule([
      {
        id: '0',
        color: Color.kBlue,
        title: 'Group 1',
        updateTime: 'Recently used',
        deviceName: null,
        faviconUrls: [{url: 'https://www.google.com'}],
        totalTabCount: 1,
        isSharedTabGroup: false,
      },
      {
        id: '1',
        color: Color.kBlue,
        title: 'Group 2',
        updateTime: 'Recently used',
        deviceName: null,
        faviconUrls: [],
        totalTabCount: 0,
        isSharedTabGroup: false,
      },
    ]);
    assertTrue(!!module);

    const groups =
        module.shadowRoot.querySelectorAll<HTMLAnchorElement>('.tab-group');
    assertEquals(2, groups.length);

    const index = 1;

    // Act.
    handler.setResultFor('openTabGroup', Promise.resolve());
    groups[index]!.click();
    await microtasksFinished();

    // Assert.
    assertEquals(1, handler.getCallCount('openTabGroup'));
    assertEquals(
        1, metrics.count('NewTabPage.TabGroups.ClickTabGroupIndex', index));
    const groupId = handler.getArgs('openTabGroup')[0];
    assertEquals(`${index}`, groupId);
  });

  test('do not show zero-state card when there are no tab groups', async () => {
    // Arrange.
    const module = await createModule([]);

    // Assert.
    assertFalse(!!module);
  });

  test('compute aria lables for tab groups', async () => {
    // Arrange.
    const pluralString = new TestPluralStringProxy();
    pluralString.text = '2 tabs';
    NTPPluralStringProxyImpl.setInstance(pluralString);

    const module = await createModule([
      {
        id: '0',
        color: Color.kBlue,
        title: 'Group 1',
        updateTime: 'Recently used',
        deviceName: 'Test Device',
        faviconUrls: [],
        totalTabCount: 2,
        isSharedTabGroup: true,
      },
    ]);
    assertTrue(!!module);

    const groups =
        module.shadowRoot.querySelectorAll<CrButtonElement>('.tab-group');
    assertEquals(1, groups.length);

    // Assert.
    const expectedLabel = '2 tabs Group 1 Recently used • Test Device shared';
    assertEquals(expectedLabel, groups[0]!.getAttribute('aria-label'));
  });

  suite('with zero state flag enabled', () => {
    test('show zero state card when there are no tab groups', async () => {
      // Arrange.
      const module = await createModule([], true);

      // Assert.
      // The module must still exist without tab groups data.
      assertTrue(!!module);
      assertTrue(
          isVisible(module.shadowRoot.querySelector('ntp-module-header')));

      // The zero-state container should be present and visible.
      const zeroStateContainer =
          module.shadowRoot.querySelector('#zeroTabGroupsContainer');
      assertTrue(!!zeroStateContainer);
      assertTrue(isVisible(zeroStateContainer));

      // Tab group rows should be absent.
      const tabGroupContainers =
          module.shadowRoot.querySelectorAll('.tab-group-container');
      assertEquals(0, tabGroupContainers.length);

      // Verify strings.
      assertEquals(
          'Stay organized with tab groups',
          zeroStateContainer.querySelector(
                                '#zeroTabGroupsTitle')!.textContent.trim());
      assertEquals(
          'You can group tabs to keep related pages together and saved across your devices',
          zeroStateContainer.querySelector(
                                '#zeroTabGroupsText')!.textContent.trim());
    });

    test('create new tab group from the zero state card', async () => {
      // Arrange.
      const module = await createModule([], true);
      assertTrue(!!module);

      const createNewTabGroupButton =
          module.shadowRoot.querySelector<HTMLButtonElement>(
              '#createNewTabGroupButton');
      assertTrue(!!createNewTabGroupButton);
      assertTrue(isVisible(createNewTabGroupButton));

      // Act.
      handler.setResultFor('createNewTabGroup', Promise.resolve());
      createNewTabGroupButton.click();
      await microtasksFinished();

      // Assert.
      assertEquals(1, handler.getCallCount('createNewTabGroup'));
      assertEquals(1, metrics.count('NewTabPage.TabGroups.CreateNewTabGroup'));
      assertEquals(
          1, metrics.count('NewTabPage.TabGroups.CreateNewTabGroup.ZeroState'));
    });
  });
});
