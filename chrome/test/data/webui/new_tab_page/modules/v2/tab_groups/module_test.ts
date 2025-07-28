// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconContainerElement, TabGroupsModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {tabGroupsDescriptor, TabGroupsProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import type {TabGroup} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('NewTabPageModulesTabGroupsModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    handler = installMock(
        PageHandlerRemote,
        mock => TabGroupsProxyImpl.setInstance(new TabGroupsProxyImpl(mock)));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function createModule(tabGroups: TabGroup[]):
      Promise<TabGroupsModuleElement> {
    handler.setResultFor('getTabGroups', Promise.resolve({tabGroups}));
    const module =
        await tabGroupsDescriptor.initialize(0) as TabGroupsModuleElement;
    document.body.append(module);
    await microtasksFinished();
    return module;
  }

  test('no module created if no tab groups data', async () => {
    const module = await createModule([]);
    assertEquals(null, module);
  });

  test('create module', async () => {
    // Arrange.
    const tabGroups: TabGroup[] = [
      {
        title: 'Tab Group 1',
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 4,
      },
      {
        title: 'Tab Group 2',
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 8,
      },
      {
        title: 'Tab Group 3',
        faviconUrls: [
          {url: 'https://www.google.com'},
          {url: 'https://www.youtube.com'},
          {url: 'https://www.wikipedia.org'},
          {url: 'https://maps.google.com'},
        ],
        totalTabCount: 188,
      },
    ];
    const module = await createModule(tabGroups);

    // Assert.
    // Verify the module was created and is visible.
    assertTrue(!!module);
    assertTrue(
        isVisible(module.shadowRoot.querySelector('ntp-module-header-v2')));

    // Verify the tab groups info is correct.
    const groups =
        module.shadowRoot.querySelectorAll<HTMLAnchorElement>('.tab-group');
    assertTrue(!!groups);
    assertEquals(tabGroups.length, groups.length);

    // Verify tab group 1.
    for (let i = 0; i < groups.length; ++i) {
      assertEquals(
          `Tab Group ${i + 1}`,
          groups[i]!.querySelector('.tab-group-title')!.textContent);
      const iconContainer =
          groups[i]!.querySelector<IconContainerElement>('ntp-icon-container')!;
      assertTrue(!!iconContainer);
      assertDeepEquals(
          tabGroups[i]!.faviconUrls.map(u => u.url), iconContainer.faviconUrls);
      assertEquals(tabGroups[i]!.totalTabCount, iconContainer.totalTabCount);
    }
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
      title: 'Tab Group',
      faviconUrls: [{url: 'https://www.google.com'}],
      totalTabCount: 1,
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
      title: 'Tab Group',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 4,
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
      title: 'Tab Group',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 8,
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

    const overflowText = overflowCells[0]!.textContent!.trim();
    assertEquals('+5', overflowText);
  });

  test('caps at 99+ when more than 99 tabs remain', async () => {
    // Arrange.
    const module = await createModule([{
      title: 'Tab Group',
      faviconUrls: [
        {url: 'https://www.google.com'},
        {url: 'https://www.youtube.com'},
        {url: 'https://www.wikipedia.org'},
        {url: 'https://maps.google.com'},
      ],
      totalTabCount: 188,
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

    const overflowText = overflowCells[0]!.textContent!.trim();
    assertEquals('99+', overflowText);
  });
});
