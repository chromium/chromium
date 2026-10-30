// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {TabPickerAppElement} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import {TabPickerBrowserProxyImpl, TabPickerUnboundedPresentationResult} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import type {TabPickerBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_extension/tab_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

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
    isLoading: false,
    title,
    url,
    lastActive: {internalValue: 0n},
  };
}

suite('TabPickerTest', () => {
  let app: TabPickerAppElement;
  let testProxy: TestTabPickerBrowserProxy;
  let extensionBrowserProxy: TestExtensionBrowserProxy;
  let recordedMetrics:
      Array<{metricName: string, value: number, enumSize: number}> = [];
  let originalRecordEnumerationValue: any;
  let recordedBooleans: Array<{metricName: string, value: boolean}> = [];
  let originalRecordBoolean: any;
  let recordedTimes: Array<{metricName: string, value: number}> = [];
  let originalRecordMediumTime: any;

  const tab0 = createTabInfo(1, 'Tab One', 'https://example.com/1');
  const tab1 = createTabInfo(2, 'Tab Two', 'https://example.com/2');
  const tab2 = createTabInfo(3, 'Tab Three', 'https://example.com/3');
  const mockTabs: TabInfo[] = [tab0, tab1, tab2];

  suiteSetup(() => {
    // tab-picker-app opens and closes its menu on hover, and cr-action-menu
    // focuses items on mouseover. The bot's real mouse cursor can rest over
    // the test window (seen on win-rel), so Blink dispatches trusted hover
    // events whenever the trigger or menu is laid out under it. These race
    // with the synthetic events the tests dispatch and flip the menu state.
    // Disable hit testing for the page so the real cursor never targets it.
    // dispatchEvent() and click() are not affected by `pointer-events`.
    document.body.style.pointerEvents = 'none';

    (window as any).chrome = (window as any).chrome || {};
    (window as any).chrome.histograms = (window as any).chrome.histograms || {};
    originalRecordEnumerationValue =
        (window as any).chrome.histograms.recordEnumerationValue;
    (window as any).chrome.histograms.recordEnumerationValue =
        (metricName: string, value: number, enumSize: number) => {
          recordedMetrics.push({metricName, value, enumSize});
          if (originalRecordEnumerationValue) {
            originalRecordEnumerationValue(metricName, value, enumSize);
          }
        };
    originalRecordBoolean = (window as any).chrome.histograms.recordBoolean;
    (window as any).chrome.histograms.recordBoolean =
        (metricName: string, value: boolean) => {
          recordedBooleans.push({metricName, value});
          if (originalRecordBoolean) {
            originalRecordBoolean(metricName, value);
          }
        };
    originalRecordMediumTime =
        (window as any).chrome.histograms.recordMediumTime;
    (window as any).chrome.histograms.recordMediumTime =
        (metricName: string, value: number) => {
          recordedTimes.push({metricName, value});
          if (originalRecordMediumTime) {
            originalRecordMediumTime(metricName, value);
          }
        };
  });

  suiteTeardown(() => {
    document.body.style.pointerEvents = '';

    if ((window as any).chrome && (window as any).chrome.histograms) {
      (window as any).chrome.histograms.recordEnumerationValue =
          originalRecordEnumerationValue;
      (window as any).chrome.histograms.recordBoolean = originalRecordBoolean;
      (window as any).chrome.histograms.recordMediumTime =
          originalRecordMediumTime;
    }
  });

  setup(async () => {
    recordedMetrics = [];
    recordedBooleans = [];
    recordedTimes = [];
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Stub the Unbounded Element API in unit tests so opening the menu does
    // not create an OS-level UnboundedSurfaceWindow (which can be dismissed by
    // background window focus changes when browser_tests run in parallel),
    // while setting visibility: visible to override the UA stylesheet rule
    // `[unbounded]:not(:unbounded) { visibility: hidden }` so menu items remain
    // focusable.
    (HTMLElement.prototype as unknown as
     Record<string, unknown>)['showUnboundedElement'] = function(
        this: HTMLElement) {
      this.style.visibility = 'visible';
      return Promise.resolve();
    };
    (HTMLElement.prototype as unknown as
     Record<string, unknown>)['hideUnboundedElement'] = function(
        this: HTMLElement) {
      this.style.visibility = '';
      return Promise.resolve();
    };

    loadTimeData.overrideValues({
      shareTabs: 'Add tabs',
      recentTabsSuffix: 'Recent',
      contextualTasksUnboundedMenuEnabled: true,
    });

    extensionBrowserProxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(extensionBrowserProxy);

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

    const triggerStyle = window.getComputedStyle(app.$.shareTabsTrigger);
    assertEquals('Arial, sans-serif', triggerStyle.fontFamily);
    assertEquals('14px', triggerStyle.fontSize);
    assertEquals(
        '20px', triggerStyle.getPropertyValue('--iron-icon-height').trim());
    assertEquals(
        '20px', triggerStyle.getPropertyValue('--iron-icon-width').trim());
  });

  test(
      'Opens tab menu on pointerenter and closes on pointerleave with delay',
      async () => {
        assertFalse(app.$.tabMenu.open);

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);

        const originalSetTimeout = window.setTimeout.bind(window);
        const scheduledTimeouts: Array<number|undefined> = [];
        window.setTimeout =
            ((handler: TimerHandler, timeout?: number, ...args: unknown[]) => {
              scheduledTimeouts.push(timeout);
              return originalSetTimeout(handler, timeout, ...args);
            }) as typeof window.setTimeout;

        try {
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerleave'));
          assertEquals(1, scheduledTimeouts.length);
          assertEquals(50, scheduledTimeouts[0]);
          // Menu remains open immediately after pointer leave due to close
          // delay.
          assertTrue(app.$.tabMenu.open);

          // Cancel timer with pointerenter on trigger.
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerenter'));
          assertTrue(app.$.tabMenu.open);

          // Handoff from trigger to menu cancels close timer, and leaving menu
          // schedules close timer.
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerleave'));
          assertEquals(2, scheduledTimeouts.length);
          assertEquals(50, scheduledTimeouts[1]);
          app.$.tabMenu.dispatchEvent(new PointerEvent('pointerenter'));
          await new Promise(resolve => originalSetTimeout(resolve, 60));
          assertTrue(app.$.tabMenu.open);

          const closePromise = eventToPromise('close', app.$.tabMenu);
          app.$.tabMenu.dispatchEvent(new PointerEvent('pointerleave'));
          assertEquals(3, scheduledTimeouts.length);
          assertEquals(50, scheduledTimeouts[2]);
          assertTrue(app.$.tabMenu.open);
          await closePromise;
          assertFalse(app.$.tabMenu.open);
        } finally {
          window.setTimeout = originalSetTimeout;
        }
      });

  test(
      'Does not open unbounded menu if pointer leaves while setUnbounded ' +
          'is pending',
      async () => {
        const dialog = app.$.tabMenu.getDialog();
        dialog.removeAttribute('unbounded');

        let setUnboundedResolver = new PromiseResolver<boolean>();
        const originalSetUnbounded =
            app.$.tabMenu.setUnbounded.bind(app.$.tabMenu);
        const originalShowAt = app.$.tabMenu.showAt.bind(app.$.tabMenu);
        let showAtCalls = 0;
        app.$.tabMenu.setUnbounded = async () => {
          const result = await setUnboundedResolver.promise;
          dialog.setAttribute('unbounded', '');
          return result;
        };
        app.$.tabMenu.showAt = (...args) => {
          showAtCalls++;
          originalShowAt(...args);
        };

        try {
          // Pointer enters and then leaves while setUnbounded() is still
          // pending.
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerenter'));
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerleave'));

          // Wait for the close timer to fire while setUnbounded() is still in
          // flight, then resolve setUnbounded().
          await new Promise(resolve => window.setTimeout(resolve, 60));
          setUnboundedResolver.resolve(true);
          await microtasksFinished();

          assertFalse(app.$.tabMenu.open);
          assertFalse(app.tabMenuOpen);
          assertEquals(0, showAtCalls);

          // Concurrent pointerenter and keyboard activation after a completed
          // close timer open the menu once when setUnbounded() resolves.
          dialog.removeAttribute('unbounded');
          setUnboundedResolver = new PromiseResolver<boolean>();
          app.$.shareTabsTrigger.dispatchEvent(
              new PointerEvent('pointerenter'));
          app.$.shareTabsTrigger.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          setUnboundedResolver.resolve(true);
          await microtasksFinished();

          assertTrue(app.$.tabMenu.open);
          assertTrue(app.tabMenuOpen);
          assertEquals(1, showAtCalls);
        } finally {
          app.$.tabMenu.setUnbounded = originalSetUnbounded;
          app.$.tabMenu.showAt = originalShowAt;
        }
      });

  test(
      'Clears focus and active state when pointer leaves and menu closes',
      async () => {
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);
        assertEquals(
            'true', app.$.shareTabsTrigger.getAttribute('aria-expanded'));

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerleave'));
        assertFalse(document.activeElement === app.$.shareTabsTrigger);

        const closePromise = eventToPromise('close', app.$.tabMenu);
        app.$.tabMenu.close();
        await closePromise;
        await new Promise(resolve => requestAnimationFrame(resolve));
        await microtasksFinished();

        assertFalse(app.$.tabMenu.open);
        assertEquals(
            'false', app.$.shareTabsTrigger.getAttribute('aria-expanded'));
        assertFalse(document.activeElement === app.$.shareTabsTrigger);
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

  test('Cycles through tab menu items with Tab and Shift+Tab', async () => {
    app.$.shareTabsTrigger.focus();
    app.$.shareTabsTrigger.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);

    const items =
        app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
    assertEquals(3, items.length);
    assertEquals(items[0], app.shadowRoot.activeElement);

    // Tab forward through items and wrap around.
    items[0]!.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Tab', bubbles: true}));
    assertTrue(app.$.tabMenu.open);
    assertEquals(items[1], app.shadowRoot.activeElement);

    items[1]!.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Tab', bubbles: true}));
    assertTrue(app.$.tabMenu.open);
    assertEquals(items[2], app.shadowRoot.activeElement);

    items[2]!.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Tab', bubbles: true}));
    assertTrue(app.$.tabMenu.open);
    assertEquals(items[0], app.shadowRoot.activeElement);

    // Shift+Tab backward and wrap around.
    items[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', shiftKey: true, bubbles: true}));
    assertTrue(app.$.tabMenu.open);
    assertEquals(items[2], app.shadowRoot.activeElement);

    // Escape closes the menu and restores focus to trigger.
    items[2]!.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    await microtasksFinished();
    assertFalse(app.$.tabMenu.open);
    assertEquals(app.$.shareTabsTrigger, app.shadowRoot.activeElement);
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
    assertEquals(2, app.selectedTabs.length);
    assertEquals(tab1.tabId, app.selectedTabs[0]!.tabId);
    assertEquals(tab0.tabId, app.selectedTabs[1]!.tabId);

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

    assertFalse(app.$.tabMenu.getDialog().hasAttribute('unbounded'));
  });

  test(
      'Flips unbounded flyout menu to the left when near right screen edge',
      async () => {
        app.style.width = '200px';
        const availLeft =
            (window.screen as Screen & {availLeft?: number}).availLeft ?? 0;
        const availTop =
            (window.screen as Screen & {availTop?: number}).availTop ?? 0;
        const dialog = app.$.tabMenu.getDialog();

        // Simulate trigger near the left side of the screen: menu opens to the
        // right of the trigger.
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter', {
          screenX: availLeft + 100,
          clientX: 50,
          screenY: availTop + 200,
          clientY: 10,
        }));
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);
        assertTrue(parseFloat(dialog.style.left) >= 0);

        app.$.tabMenu.close();
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);

        // Simulate trigger near the right edge of the screen (side panel docked
        // on the right): menu flips to the left of the trigger.
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter', {
          screenX: availLeft + window.screen.availWidth - 50,
          clientX: 50,
          screenY: availTop + 200,
          clientY: 10,
        }));
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);
        assertTrue(parseFloat(dialog.style.left) < 0);
      });

  test(
      'Tab selection remains sticky across menu close and reopen', async () => {
        app.$.shareTabsTrigger.click();
        await microtasksFinished();

        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        const [item0] = items;
        assert(item0);

        item0.click();
        await microtasksFinished();

        assertTrue(isVisible(item0.querySelector('.share-tabs-check')));

        app.$.tabMenu.close();
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);

        app.$.shareTabsTrigger.click();
        await microtasksFinished();
        assertTrue(app.$.tabMenu.open);

        const reopenedItems =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        const [reopenedItem0] = reopenedItems;
        assert(reopenedItem0);
        assertTrue(isVisible(reopenedItem0.querySelector('.share-tabs-check')));
      });

  test('Dark mode reflects attribute correctly', async () => {
    assertFalse(app.darkMode);
    assertFalse(app.hasAttribute('dark-mode'));

    app.darkMode = true;
    await microtasksFinished();
    assertTrue(app.hasAttribute('dark-mode'));

    app.darkMode = false;
    await microtasksFinished();
    assertFalse(app.hasAttribute('dark-mode'));
  });

  test('Flyout item text color adapts in dark mode', async () => {
    const tab3 = createTabInfo(4, 'Tab Four', 'https://example.com/4');
    app.selectedTabs = [tab0, tab1, tab2, tab3];
    await microtasksFinished();

    app.$.shareTabsTrigger.click();
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);

    const dialog = app.$.tabMenu.getDialog();
    const items =
        app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
    const [item0] = items;
    assert(item0);
    const faviconGroup =
        app.$.shareTabsTrigger.querySelector('composebox-favicon-group');
    assert(faviconGroup);
    const faviconItem =
        faviconGroup.shadowRoot.querySelector<HTMLElement>('.favicon-item');
    assert(faviconItem);
    const moreItems =
        faviconGroup.shadowRoot.querySelector<HTMLElement>('#more-items');
    assert(moreItems);

    assertEquals('light', window.getComputedStyle(app).colorScheme);
    assertEquals(
        'rgb(255, 255, 255)', window.getComputedStyle(dialog).backgroundColor);
    assertEquals('rgb(31, 31, 31)', window.getComputedStyle(item0).color);
    assertEquals(
        'rgba(0, 0, 0, 0.04)',
        window.getComputedStyle(app.$.shareTabsTrigger).backgroundColor);
    assertEquals(
        '#0b57d0',
        window.getComputedStyle(app)
            .getPropertyValue('--cr-focus-outline-color')
            .trim());
    assertEquals(
        'rgb(255, 255, 255)', window.getComputedStyle(faviconItem).borderColor);
    assertEquals('rgb(71, 71, 71)', window.getComputedStyle(moreItems).color);

    app.darkMode = true;
    await microtasksFinished();

    assertEquals('dark', window.getComputedStyle(app).colorScheme);
    assertEquals(
        'rgb(44, 46, 54)', window.getComputedStyle(dialog).backgroundColor);
    assertEquals('rgb(230, 232, 240)', window.getComputedStyle(item0).color);
    assertEquals(
        'rgb(53, 53, 53)',
        window.getComputedStyle(app.$.shareTabsTrigger).backgroundColor);
    assertEquals(
        '#a8c7fa',
        window.getComputedStyle(app)
            .getPropertyValue('--cr-focus-outline-color')
            .trim());
    assertEquals(
        'rgb(44, 46, 54)', window.getComputedStyle(faviconItem).borderColor);
    assertEquals(
        'rgb(199, 199, 199)', window.getComputedStyle(moreItems).color);
  });

  test('Initializes darkMode from cs URL query param', async () => {
    const originalUrl = window.location.href;
    let lightApp: TabPickerAppElement|undefined;
    let darkApp: TabPickerAppElement|undefined;
    try {
      window.history.replaceState({}, '', '?cs=0');
      lightApp = document.createElement('tab-picker-app');
      document.body.appendChild(lightApp);
      await microtasksFinished();
      assertFalse(lightApp.darkMode);
      assertFalse(lightApp.hasAttribute('dark-mode'));
      assertEquals('light', window.getComputedStyle(lightApp).colorScheme);

      window.history.replaceState({}, '', '?cs=1');
      darkApp = document.createElement('tab-picker-app');
      document.body.appendChild(darkApp);
      await microtasksFinished();
      assertTrue(darkApp.darkMode);
      assertTrue(darkApp.hasAttribute('dark-mode'));
      assertEquals('dark', window.getComputedStyle(darkApp).colorScheme);
    } finally {
      lightApp?.remove();
      darkApp?.remove();
      window.history.replaceState({}, '', originalUrl);
    }
  });

  test('Flyout items and recent suffix use Arial', async () => {
    app.$.shareTabsTrigger.click();
    await microtasksFinished();
    assertTrue(app.$.tabMenu.open);

    const [item0] =
        app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
    assert(item0);
    const recentSuffix =
        item0.querySelector<HTMLElement>('.recent-tabs-suffix');
    assert(recentSuffix);

    assertEquals(
        'Arial, sans-serif', window.getComputedStyle(item0).fontFamily);
    assertEquals(
        'Arial, sans-serif', window.getComputedStyle(recentSuffix).fontFamily);
  });

  test(
      'Records ContextualSearch.TabPicker.SelectedTabPosition.ContextualTasksExtension',
      async () => {
        app.tabSuggestions = [tab0, tab1, tab2];
        await microtasksFinished();

        app.$.shareTabsTrigger.click();
        await microtasksFinished();

        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(3, items.length);

        for (const expectedPosition of [0, 1, 2]) {
          recordedMetrics = [];
          recordedBooleans = [];
          items[expectedPosition]!.click();
          await microtasksFinished();

          const position = recordedMetrics.find(
              m => m.metricName ===
                  'ContextualSearch.TabPicker.SelectedTabPosition.ContextualTasksExtension');
          assertTrue(!!position);
          assertEquals(expectedPosition, position.value);
          assertEquals(21, position.enumSize);

          // No active tab candidate exists in suggestions, so
          // SelectedTabIsActive is omitted.
          assertFalse(!!recordedBooleans.find(
              m => m.metricName ===
                  'ContextualSearch.TabPicker.SelectedTabIsActive.ContextualTasksExtension'));

          const tabSelectedAction = recordedBooleans.find(
              m => m.metricName ===
                  'ContextualTasks.TabPicker.UserAction.TabSelected');
          assertTrue(!!tabSelectedAction);
          assertTrue(tabSelectedAction.value);
          assertFalse(!!recordedBooleans.find(
              m => m.metricName ===
                  'ContextualTasks.TabPicker.UserAction.TabDeselected'));
        }

        // Deselecting a tab should not record ContextualSearch.TabPicker.*,
        // and should record ContextualTasks.TabPicker.UserAction.TabDeselected.
        recordedMetrics = [];
        recordedBooleans = [];
        items[0]!.click();
        await microtasksFinished();

        assertFalse(!!recordedMetrics.find(
            m => m.metricName.startsWith('ContextualSearch.TabPicker.')));
        assertFalse(!!recordedBooleans.find(
            m => m.metricName.startsWith('ContextualSearch.TabPicker.')));
        const tabDeselectedAction = recordedBooleans.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.UserAction.TabDeselected');
        assertTrue(!!tabDeselectedAction);
        assertTrue(tabDeselectedAction.value);
        assertFalse(!!recordedBooleans.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.UserAction.TabSelected'));
      });

  test(
      'Records ContextualSearch.TabPicker.SelectedTabIsActive.ContextualTasksExtension independently of position',
      async () => {
        const recentTab =
            createTabInfo(1, 'Recent Tab', 'https://example.com/recent');
        const activeTab =
            createTabInfo(10, 'Active Tab', 'https://example.com/active');
        activeTab.showInCurrentTabChip = true;

        // Deliberately put the active tab at position 1 rather than 0, so that
        // the two axes disagree and the test would fail if they were conflated.
        testProxy.setRecentTabs([recentTab, activeTab]);
        app.tabSuggestions = [recentTab, activeTab];
        app.recentTabId = recentTab.tabId;
        await microtasksFinished();

        app.$.shareTabsTrigger.click();
        await microtasksFinished();

        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(2, items.length);

        // Position 0, not the active tab.
        recordedMetrics = [];
        recordedBooleans = [];
        items[0]!.click();
        await microtasksFinished();

        let isActive = recordedBooleans.find(
            m => m.metricName ===
                'ContextualSearch.TabPicker.SelectedTabIsActive.ContextualTasksExtension');
        assertTrue(!!isActive);
        assertFalse(isActive.value);
        assertEquals(
            0,
            recordedMetrics
                .find(
                    m => m.metricName ===
                        'ContextualSearch.TabPicker.SelectedTabPosition.ContextualTasksExtension')
                ?.value);

        // Position 1, and it is the active tab.
        recordedMetrics = [];
        recordedBooleans = [];
        items[1]!.click();
        await microtasksFinished();

        isActive = recordedBooleans.find(
            m => m.metricName ===
                'ContextualSearch.TabPicker.SelectedTabIsActive.ContextualTasksExtension');
        assertTrue(!!isActive);
        assertTrue(isActive.value);
        assertEquals(
            1,
            recordedMetrics
                .find(
                    m => m.metricName ===
                        'ContextualSearch.TabPicker.SelectedTabPosition.ContextualTasksExtension')
                ?.value);
      });

  test(
      'Repopulates selected tabs when onTabContextUpdated is dispatched',
      async () => {
        extensionBrowserProxy.callbackRouterRemote.onTabContextUpdated(
            [tab0, tab2], []);
        await extensionBrowserProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertEquals(2, app.selectedTabs.length);
        assertEquals(tab0.tabId, app.selectedTabs[0]!.tabId);
        assertEquals(tab2.tabId, app.selectedTabs[1]!.tabId);

        const label = app.$.shareTabsTrigger.querySelector('.tab-title');
        assert(label);
        assertEquals('Sharing 2 tabs', label.textContent.trim());

        app.$.shareTabsTrigger.click();
        await microtasksFinished();

        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(3, items.length);
        assertTrue(isVisible(items[0]!.querySelector('.share-tabs-check')));
        assertFalse(isVisible(items[1]!.querySelector('.share-tabs-check')));
        assertTrue(isVisible(items[2]!.querySelector('.share-tabs-check')));

        // Deselecting a repopulated tab deletes it from context.
        items[0]!.click();
        const deletedTabId = await testProxy.whenCalled('deleteTabContext');
        assertEquals(tab0.tabId, deletedTabId);
        await microtasksFinished();
        assertEquals('Sharing 1 tab', label.textContent.trim());

        // If onTabContextUpdated fires while the tab menu is open (e.g. an
        // attached tab navigates or closes), tab suggestions are refreshed.
        const updatedTab2 =
            createTabInfo(3, 'Tab Three Navigated', 'https://example.com/3b');
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([tab1, updatedTab2]);
        extensionBrowserProxy.callbackRouterRemote.onTabContextUpdated([], []);
        await extensionBrowserProxy.callbackRouterRemote.$.flushForTesting();
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        const refreshedItems =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(2, refreshedItems.length);
        assertEquals(
            'Tab Three Navigated',
            refreshedItems[1]!.querySelector('.tab-title')!.textContent.trim());

        // If all eligible tabs are closed while the menu is open, the menu
        // closes automatically.
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([]);
        extensionBrowserProxy.callbackRouterRemote.onTabContextUpdated([], []);
        await extensionBrowserProxy.callbackRouterRemote.$.flushForTesting();
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);
      });

  test(
      'Hides focus outline on pointer hover and shows on keyboard navigation',
      async () => {
        // Focus outlines start hidden when the element connects.
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));

        // Focusing the trigger row without a pointer press (e.g. via Tab)
        // enables the focus outline on the trigger button.
        app.$.shareTabsTrigger.focus();
        if (!document.hasFocus()) {
          app.$.shareTabsTrigger.dispatchEvent(new FocusEvent('focus'));
        }
        assertEquals(app.$.shareTabsTrigger, app.shadowRoot.activeElement);
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        if (document.hasFocus()) {
          assertEquals(
              'solid',
              window.getComputedStyle(app.$.shareTabsTrigger).outlineStyle);
        }

        // Opening via pointerenter clears focus outlines and does not
        // auto-focus the first tab item.
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertTrue(app.$.tabMenu.open);
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals(
            'none',
            window.getComputedStyle(app.$.shareTabsTrigger).outlineStyle);

        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        const [item0, item1] = items;
        assert(item0);
        assert(item1);
        assertFalse(app.shadowRoot.activeElement === item0);
        assertEquals('none', window.getComputedStyle(item0).outlineStyle);

        // Hovering a tab item focuses it via cr-action-menu mouseover without
        // showing a keyboard focus outline.
        item0.dispatchEvent(
            new MouseEvent('mouseover', {bubbles: true, composed: true}));
        await microtasksFinished();
        assertEquals(item0, app.shadowRoot.activeElement);
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals('none', window.getComputedStyle(item0).outlineStyle);

        const closePromise1 = eventToPromise('close', app.$.tabMenu);
        app.$.tabMenu.close();
        await closePromise1;
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));

        // Pointer press on the trigger row suppresses the focus outline when
        // the browser focuses the button on pointerdown, and clicking clears
        // focus-outline-visible.
        app.$.shareTabsTrigger.focus();
        if (!document.hasFocus()) {
          app.$.shareTabsTrigger.dispatchEvent(new FocusEvent('focus'));
        }
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        app.$.shareTabsTrigger.blur();
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerdown'));
        app.$.shareTabsTrigger.focus();
        if (!document.hasFocus()) {
          app.$.shareTabsTrigger.dispatchEvent(new FocusEvent('focus'));
        }
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));

        app.$.shareTabsTrigger.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Tab', bubbles: true}));
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        app.$.shareTabsTrigger.click();
        await microtasksFinished();
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertTrue(app.$.tabMenu.open);
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertFalse(app.shadowRoot.activeElement === item0);

        const closePromise2 = eventToPromise('close', app.$.tabMenu);
        app.$.tabMenu.close();
        await closePromise2;
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);

        // Opening via keyboard enables focus outlines and focuses the first
        // tab item.
        app.$.shareTabsTrigger.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
        await microtasksFinished();
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertTrue(app.$.tabMenu.open);
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals(item0, app.shadowRoot.activeElement);
        if (document.hasFocus()) {
          assertEquals('solid', window.getComputedStyle(item0).outlineStyle);
        }

        // Moving the pointer over the menu clears the keyboard focus outline.
        app.$.tabMenu.dispatchEvent(
            new PointerEvent('pointermove', {bubbles: true}));
        await microtasksFinished();
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals('none', window.getComputedStyle(item0).outlineStyle);

        // Pressing ArrowDown inside the menu re-enables the keyboard focus
        // outline and advances focus to the second tab item.
        app.$.tabMenu.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals(item1, app.shadowRoot.activeElement);
        if (document.hasFocus()) {
          assertEquals('solid', window.getComputedStyle(item1).outlineStyle);
        }

        // Pressing ArrowLeft inside the menu closes the cascading submenu and
        // preserves keyboard focus and outline on the trigger button.
        const arrowLeftClosePromise = eventToPromise('close', app.$.tabMenu);
        app.$.tabMenu.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowLeft', bubbles: true}));
        await arrowLeftClosePromise;
        await new Promise(resolve => requestAnimationFrame(resolve));
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);
        assertEquals(app.$.shareTabsTrigger, app.shadowRoot.activeElement);
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));
        if (document.hasFocus()) {
          assertEquals(
              'solid',
              window.getComputedStyle(app.$.shareTabsTrigger).outlineStyle);
        }

        // Pressing Escape inside the menu also preserves keyboard focus and
        // outline on the trigger button.
        app.$.shareTabsTrigger.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
        await microtasksFinished();
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertTrue(app.$.tabMenu.open);

        const escapeClosePromise = eventToPromise('close', app.$.tabMenu);
        app.$.tabMenu.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
        await escapeClosePromise;
        await new Promise(resolve => requestAnimationFrame(resolve));
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);
        assertEquals(app.$.shareTabsTrigger, app.shadowRoot.activeElement);
        assertTrue(document.documentElement.classList.contains(
            'focus-outline-visible'));

        // Moving the pointer over the trigger row clears the keyboard focus
        // outline.
        app.$.shareTabsTrigger.dispatchEvent(
            new PointerEvent('pointermove', {bubbles: true}));
        await microtasksFinished();
        assertFalse(document.documentElement.classList.contains(
            'focus-outline-visible'));
        assertEquals(
            'none',
            window.getComputedStyle(app.$.shareTabsTrigger).outlineStyle);
      });

  test(
      'Refreshes tab suggestions when hovering over trigger or menu',
      async () => {
        const newTab = createTabInfo(4, 'Tab Four', 'https://example.com/4');
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([newTab, ...mockTabs]);

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        assertTrue(app.$.tabMenu.open);
        assertEquals(newTab.tabId, app.recentTabId);
        let items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(4, items.length);
        assertEquals(
            'Tab Four',
            items[0]!.querySelector('.tab-title')!.textContent.trim());
        assertTrue(isVisible(items[0]!.querySelector('.recent-tabs-suffix')));
        assertFalse(isVisible(items[1]!.querySelector('.recent-tabs-suffix')));

        // While the menu remains open, another tab is opened and the pointer
        // enters the menu.
        const newerTab = createTabInfo(5, 'Tab Five', 'https://example.com/5');
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([newerTab, newTab, ...mockTabs]);

        app.$.tabMenu.dispatchEvent(new PointerEvent('pointerenter'));
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        assertEquals(newerTab.tabId, app.recentTabId);
        items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(5, items.length);
        assertEquals(
            'Tab Five',
            items[0]!.querySelector('.tab-title')!.textContent.trim());
        assertTrue(isVisible(items[0]!.querySelector('.recent-tabs-suffix')));

        // If all eligible tabs are closed before hovering the menu, the menu
        // closes and clears recentTabId.
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([]);

        app.$.tabMenu.dispatchEvent(new PointerEvent('pointerenter'));
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        assertFalse(app.$.tabMenu.open);
        assertEquals(null, app.recentTabId);
      });

  test(
      'Opens tab menu on hover when tabs are added after empty initial load',
      async () => {
        app.remove();
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([]);

        app = document.createElement('tab-picker-app');
        document.body.appendChild(app);
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        assertEquals(0, app.tabSuggestions.length);
        assertEquals(null, app.recentTabId);

        // Hovering when there are still no tabs does not open the menu.
        testProxy.resetResolver('getRecentTabs');
        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();
        assertFalse(app.$.tabMenu.open);

        // A new tab is opened while the outer menu remains mounted, and the
        // user hovers over the trigger again.
        const newTab = createTabInfo(10, 'New Tab', 'https://example.com/new');
        testProxy.resetResolver('getRecentTabs');
        testProxy.setRecentTabs([newTab]);

        app.$.shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
        await testProxy.whenCalled('getRecentTabs');
        await microtasksFinished();

        assertTrue(app.$.tabMenu.open);
        assertEquals(newTab.tabId, app.recentTabId);
        const items =
            app.$.tabMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(1, items.length);
        assertEquals(
            'New Tab',
            items[0]!.querySelector('.tab-title')!.textContent.trim());
      });

  test(
      'Records SHOWN_UNBOUNDED and MenuOpenLatencyMs on successful open',
      async () => {
        app.tabSuggestions = mockTabs;
        await microtasksFinished();

        recordedMetrics = [];
        recordedTimes = [];
        const shownPromise = eventToPromise('unbounded-shown', app.$.tabMenu);
        await app.openTabMenuForTesting();
        await shownPromise;

        const resultMetric = recordedMetrics.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.UnboundedPresentationResult');
        assertTrue(!!resultMetric);
        assertEquals(
            TabPickerUnboundedPresentationResult.SHOWN_UNBOUNDED,
            resultMetric.value);
        assertEquals(3, resultMetric.enumSize);

        const latencyMetric = recordedTimes.find(
            m =>
                m.metricName === 'ContextualTasks.TabPicker.MenuOpenLatencyMs');
        assertTrue(!!latencyMetric);
        assertTrue(latencyMetric.value >= 0);
      });

  test(
      'Records FALLBACK_API_UNSUPPORTED and MenuOpenLatencyMs when API is unsupported',
      async () => {
        const original = (HTMLElement.prototype as any)['showUnboundedElement'];
        delete (HTMLElement.prototype as any)['showUnboundedElement'];

        try {
          const unsuppApp = document.createElement('tab-picker-app');
          document.body.appendChild(unsuppApp);
          await microtasksFinished();

          unsuppApp.tabSuggestions = mockTabs;
          await microtasksFinished();

          recordedMetrics = [];
          recordedTimes = [];
          await unsuppApp.openTabMenuForTesting();

          const resultMetric = recordedMetrics.find(
              m => m.metricName ===
                  'ContextualTasks.TabPicker.UnboundedPresentationResult');
          assertTrue(!!resultMetric);
          assertEquals(
              TabPickerUnboundedPresentationResult.FALLBACK_API_UNSUPPORTED,
              resultMetric.value);
          assertEquals(3, resultMetric.enumSize);

          const latencyMetric = recordedTimes.find(
              m => m.metricName ===
                  'ContextualTasks.TabPicker.MenuOpenLatencyMs');
          assertTrue(!!latencyMetric);
          assertTrue(latencyMetric.value >= 0);

          unsuppApp.remove();
        } finally {
          (HTMLElement.prototype as any)['showUnboundedElement'] = original;
        }
      });

  test(
      'Records SHOW_UNBOUNDED_ELEMENT_FAILED and no latency when presentation rejects',
      async () => {
        const failingApp = document.createElement('tab-picker-app');
        document.body.appendChild(failingApp);
        await microtasksFinished();

        failingApp.tabSuggestions = mockTabs;
        (failingApp.$.tabMenu.getDialog() as any).showUnboundedElement = () =>
            Promise.reject(new Error('Internal popup failure'));

        recordedMetrics = [];
        recordedTimes = [];
        const originalConsoleError = console.error;
        console.error = () => {};
        try {
          await failingApp.openTabMenuForTesting();
        } finally {
          console.error = originalConsoleError;
        }

        const resultMetric = recordedMetrics.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.UnboundedPresentationResult');
        assertTrue(!!resultMetric);
        assertEquals(
            TabPickerUnboundedPresentationResult.SHOW_UNBOUNDED_ELEMENT_FAILED,
            resultMetric.value);
        assertEquals(3, resultMetric.enumSize);

        const latencyMetric = recordedTimes.find(
            m =>
                m.metricName === 'ContextualTasks.TabPicker.MenuOpenLatencyMs');
        assertFalse(!!latencyMetric);

        failingApp.remove();
      });

  test(
      'Does not record failure or latency when showUnboundedElement rejects with AbortError',
      async () => {
        const abortApp = document.createElement('tab-picker-app');
        document.body.appendChild(abortApp);
        await microtasksFinished();

        abortApp.tabSuggestions = mockTabs;
        (abortApp.$.tabMenu.getDialog() as any).showUnboundedElement = () =>
            Promise.reject(new DOMException('User dismissed', 'AbortError'));

        recordedMetrics = [];
        recordedTimes = [];
        await abortApp.openTabMenuForTesting();

        const resultMetric = recordedMetrics.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.UnboundedPresentationResult');
        assertFalse(!!resultMetric);

        const latencyMetric = recordedTimes.find(
            m =>
                m.metricName === 'ContextualTasks.TabPicker.MenuOpenLatencyMs');
        assertFalse(!!latencyMetric);

        abortApp.remove();
      });

  test(
      'Records IframeRenderLatencyMs and SuggestionsLoadLatencyMs once on load',
      async () => {
        const renderMetrics = recordedTimes.filter(
            m => m.metricName ===
                'ContextualTasks.TabPicker.IframeRenderLatencyMs');
        assertEquals(1, renderMetrics.length);
        assertTrue(renderMetrics[0]!.value >= 0);

        const suggestionsMetrics = recordedTimes.filter(
            m => m.metricName ===
                'ContextualTasks.TabPicker.SuggestionsLoadLatencyMs');
        assertEquals(1, suggestionsMetrics.length);
        assertTrue(suggestionsMetrics[0]!.value >= renderMetrics[0]!.value);

        // Subsequent property updates should not re-record either load metric.
        recordedTimes = [];
        app.darkMode = true;
        app.selectedTabs = [tab0];
        await microtasksFinished();

        assertEquals(0, recordedTimes.length);
      });

  test(
      'Does not record SuggestionsLoadLatencyMs when getRecentTabs rejects',
      async () => {
        app.remove();
        testProxy.getRecentTabs = () => Promise.reject(new Error('Mojo error'));

        recordedTimes = [];
        const errorApp = document.createElement('tab-picker-app');
        document.body.appendChild(errorApp);
        await microtasksFinished();

        const renderMetric = recordedTimes.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.IframeRenderLatencyMs');
        assertTrue(!!renderMetric);
        assertTrue(renderMetric.value >= 0);

        const suggestionsMetric = recordedTimes.find(
            m => m.metricName ===
                'ContextualTasks.TabPicker.SuggestionsLoadLatencyMs');
        assertFalse(!!suggestionsMetric);

        errorApp.remove();
      });
});
