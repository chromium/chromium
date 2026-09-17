// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';

import type {TabPickerAppElement} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import {TabPickerBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import type {TabPickerBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestTabPickerBrowserProxy extends TestBrowserProxy implements
    TabPickerBrowserProxy {
  private recentTabs_: TabInfo[] = [];

  constructor() {
    super([
      'getRecentTabs',
      'getPluralString',
      'addTabContext',
      'deleteTabContext',
    ]);
  }

  setRecentTabs(tabs: TabInfo[]) {
    this.recentTabs_ = tabs;
  }

  getRecentTabs() {
    this.methodCalled('getRecentTabs');
    return Promise.resolve({tabs: this.recentTabs_});
  }

  getPluralString(messageName: string, count: number) {
    this.methodCalled('getPluralString', messageName, count);
    if (messageName === 'sharingTabs') {
      return Promise.resolve(
          count === 1 ? 'Sharing 1 tab' : `Sharing ${count} tabs`);
    }
    return Promise.resolve('');
  }

  addTabContext(tabId: number) {
    this.methodCalled('addTabContext', tabId);
  }

  deleteTabContext(tabId: number) {
    this.methodCalled('deleteTabContext', tabId);
  }
}

function createTabInfo(id: number, title: string, url: string): TabInfo {
  return {
    tabId: id,
    showInCurrentTabChip: false,
    showInPreviousTabChip: false,
    title,
    url,
    lastActive: {internalValue: 0n},
  };
}

suite('TabPickerTest', () => {
  let app: TabPickerAppElement;
  let testProxy: TestTabPickerBrowserProxy;

  const tab0 = createTabInfo(1, 'Tab One', 'https://example.com/1');
  const tab1 = createTabInfo(2, 'Tab Two', 'https://example.com/2');
  const tab2 = createTabInfo(3, 'Tab Three', 'https://example.com/3');
  const mockTabs: TabInfo[] = [tab0, tab1, tab2];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    if (!('showUnboundedElement' in HTMLElement.prototype)) {
      (HTMLElement.prototype as unknown as
       Record<string, unknown>)['showUnboundedElement'] = () =>
          Promise.resolve();
      (HTMLElement.prototype as unknown as
       Record<string, unknown>)['hideUnboundedElement'] = () =>
          Promise.resolve();
    }

    loadTimeData.overrideValues({
      shareTabs: 'Add tabs',
      recentTabsSuffix: 'Recent',
      contextualTasksUnboundedMenuEnabled: true,
    });

    testProxy = new TestTabPickerBrowserProxy();
    testProxy.setRecentTabs(mockTabs);
    TabPickerBrowserProxyImpl.setInstance(testProxy);

    app = document.createElement('tab-picker-app');
    document.body.appendChild(app);

    await testProxy.whenCalled('getRecentTabs');
    await microtasksFinished();
  });

  test('Component renders trigger button', () => {
    assertTrue(isVisible(app.$.shareTabsTrigger));
    const label = app.$.shareTabsTrigger.querySelector('.tab-title');
    assertTrue(isVisible(label));
    assert(label);
    assertEquals('Add tabs', label.textContent.trim());
  });

  test(
      'Opens tab menu on pointerenter and closes on pointerleave with delay',
      async () => {
        assertFalse(app.$.tabMenu.open);

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerleave'));
        // Menu remains open immediately after pointer leave due to close delay.
        assertTrue(app.$.tabMenu.open);

        // Cancel timer with pointerenter.
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        assertTrue(app.$.tabMenu.open);
      });

  test('Toggles tab menu on trigger click', async () => {
    assertFalse(app.$.tabMenu.open);

    app.$.shareTabsTrigger.click();
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);

    app.$.shareTabsTrigger.click();
    await microtasksFinished();
    assertFalse(app.$.tabMenu.open);
  });

  test('Opens tab menu with keyboard shortcuts', async () => {
    assertFalse(app.$.tabMenu.open);

    app.$.shareTabsTrigger.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);

    app.$.tabMenu.close();
    await microtasksFinished();
    assertFalse(app.$.tabMenu.open);

    app.$.shareTabsTrigger.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);
  });

  test('Selects tabs and updates trigger text and fires event', async () => {
    app.$.shareTabsTrigger.click();
    await microtasksFinished();

    const items =
        app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
    assertEquals(3, items.length);

    const [item0, item1] = items;
    assert(item0);
    assert(item1);

    // Verify recent tab suffix on the first tab.
    const recentSuffix = item0.querySelector('.recent-tabs-suffix');
    assertTrue(isVisible(recentSuffix));

    // Select first tab.
    const selectPromise1 =
        eventToPromise<CustomEvent<{tab: TabInfo, selected: boolean}>>(
            'tab-selected', app);
    item0.click();
    const event1 = await selectPromise1;
    assertEquals(tab0.tabId, event1.detail.tab.tabId);
    assertTrue(event1.detail.selected);
    const addedTabId1 = await testProxy.whenCalled('addTabContext');
    assertEquals(tab0.tabId, addedTabId1);
    testProxy.resetResolver('addTabContext');
    await microtasksFinished();

    const label = app.$.shareTabsTrigger.querySelector('.tab-title');
    assertTrue(isVisible(label));
    assert(label);
    assertEquals('Sharing 1 tab', label.textContent.trim());

    // Select second tab.
    const selectPromise2 =
        eventToPromise<CustomEvent<{tab: TabInfo, selected: boolean}>>(
            'tab-selected', app);
    item1.click();
    const event2 = await selectPromise2;
    assertEquals(tab1.tabId, event2.detail.tab.tabId);
    assertTrue(event2.detail.selected);
    const addedTabId2 = await testProxy.whenCalled('addTabContext');
    assertEquals(tab1.tabId, addedTabId2);
    testProxy.resetResolver('addTabContext');
    await microtasksFinished();

    assertEquals('Sharing 2 tabs', label.textContent.trim());

    // Deselect first tab.
    const deselectPromise1 =
        eventToPromise<CustomEvent<{tab: TabInfo, selected: boolean}>>(
            'tab-selected', app);
    item0.click();
    const deselectEvent1 = await deselectPromise1;
    assertEquals(tab0.tabId, deselectEvent1.detail.tab.tabId);
    assertFalse(deselectEvent1.detail.selected);
    const deletedTabId1 = await testProxy.whenCalled('deleteTabContext');
    assertEquals(tab0.tabId, deletedTabId1);
    testProxy.resetResolver('deleteTabContext');
    await microtasksFinished();

    assertEquals('Sharing 1 tab', label.textContent.trim());

    // Deselect second tab.
    const deselectPromise2 =
        eventToPromise<CustomEvent<{tab: TabInfo, selected: boolean}>>(
            'tab-selected', app);
    item1.click();
    const deselectEvent2 = await deselectPromise2;
    assertEquals(tab1.tabId, deselectEvent2.detail.tab.tabId);
    assertFalse(deselectEvent2.detail.selected);
    const deletedTabId2 = await testProxy.whenCalled('deleteTabContext');
    assertEquals(tab1.tabId, deletedTabId2);
    testProxy.resetResolver('deleteTabContext');
    await microtasksFinished();

    assertEquals('Add tabs', label.textContent.trim());
  });

  test(
      'Opens flyout menu as unbounded menu when useUnbounded is enabled',
      async () => {
        let showUnboundedCalled = false;
        let hideUnboundedCalled = false;

        const dialog = app.$.tabMenu.getDialog();
        (dialog as unknown as Record<string, unknown>)['showUnboundedElement'] =
            () => {
              showUnboundedCalled = true;
              return Promise.resolve();
            };
        (dialog as unknown as Record<string, unknown>)['hideUnboundedElement'] =
            () => {
              hideUnboundedCalled = true;
              return Promise.resolve();
            };

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();

        assertTrue(app.$.tabMenu.open);
        assertTrue(dialog.hasAttribute('unbounded'));
        assertTrue(showUnboundedCalled);

        app.$.tabMenu.close();
        await microtasksFinished();

        assertFalse(app.$.tabMenu.open);
        assertTrue(hideUnboundedCalled);
      });

  test('Closes unbounded menu on beforetoggle closed event', async () => {
    app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
    await microtasksFinished();

    assertTrue(app.$.tabMenu.open);
    assertTrue(app.$.tabMenu.getDialog().hasAttribute('unbounded'));

    const toggleEvent = new CustomEvent('beforetoggle');
    Object.assign(toggleEvent, {oldState: 'open', newState: 'closed'});
    app.$.tabMenu.getDialog().dispatchEvent(toggleEvent);
    await microtasksFinished();

    assertFalse(app.$.tabMenu.open);
  });

  test('Does not use unbounded menu when disabled', async () => {
    app.remove();

    loadTimeData.overrideValues({
      contextualTasksUnboundedMenuEnabled: false,
    });

    app = document.createElement('tab-picker-app');
    document.body.appendChild(app);
    await testProxy.whenCalled('getRecentTabs');
    await microtasksFinished();

    app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
    await microtasksFinished();

    assertTrue(app.$.tabMenu.open);
    assertFalse(app.$.tabMenu.getDialog().hasAttribute('unbounded'));
  });
});
