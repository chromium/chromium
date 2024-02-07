// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip.top-chrome/tab.js';
import 'chrome://tab-strip.top-chrome/tab_group.js';

import type {TabGroupElement} from 'chrome://tab-strip.top-chrome/tab_group.js';
import {TabsApiProxyImpl} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('TabGroup', () => {
  const groupId = 'my-group-id';

  let tabGroupElement: TabGroupElement;
  let testTabsApiProxy: TestTabsApiProxy;

  setup(() => {
    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxyImpl.setInstance(testTabsApiProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tabGroupElement = document.createElement('tabstrip-tab-group');
    tabGroupElement.dataset['groupId'] = groupId;
    tabGroupElement.appendChild(document.createElement('tabstrip-tab'));
    document.body.appendChild(tabGroupElement);
  });

  test('UpdatesVisuals', () => {
    const visuals = {
      color: '255, 0, 0',
      textColor: '0, 0, 255',
      title: 'My new title',
    };
    tabGroupElement.updateVisuals(visuals);
    assertEquals(
        visuals.title,
        tabGroupElement.shadowRoot!.querySelector<HTMLElement>(
                                       '#title')!.innerText);
    assertEquals(
        visuals.color,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-color-rgb'));
    assertEquals(
        visuals.textColor,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-text-color-rgb'));
  });

  test('DraggableChipStaysInPlace', () => {
    const chip = tabGroupElement.$('#chip') as HTMLElement;
    const originalChipRect = chip.getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = chip.getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });

  test('DraggableChipStaysInPlaceInRTL', () => {
    document.documentElement.dir = 'rtl';
    const chip = tabGroupElement.$('#chip') as HTMLElement;
    const originalChipRect = chip.getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = chip.getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });

  test('ChipOpensEditDialog', async () => {
    const chip = tabGroupElement.$('#chip') as HTMLElement;
    const chipRect = chip.getBoundingClientRect();
    chip.click();
    const [calledGroupId, locationX, locationY, width, height] =
        await testTabsApiProxy.whenCalled('showEditDialogForGroup');
    assertEquals(groupId, calledGroupId);
    assertEquals(chipRect.left, locationX);
    assertEquals(chipRect.top, locationY);
    assertEquals(chipRect.width, width);
    assertEquals(chipRect.height, height);
  });
});
