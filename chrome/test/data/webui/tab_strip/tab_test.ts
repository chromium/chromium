// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip.top-chrome/tab.js';

import {getFavicon} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabElement} from 'chrome://tab-strip.top-chrome/tab.js';
import type {Tab} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {TabNetworkState} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {CloseTabAction, TabsApiProxyImpl} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createTab, TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('Tab', function() {
  let testTabsApiProxy: TestTabsApiProxy;
  let tabElement: TabElement;

  function queryTab(): HTMLElement {
    return tabElement.shadowRoot!.querySelector<HTMLElement>('#tab')!;
  }

  const tab: Tab = createTab({
    index: 0,
    showIcon: true,
    title: 'My Title',
  });

  /**
   * Convenience function for creating a typed Tab object.
   */
  function createTabData(overrides?: Partial<Tab>): Tab {
    return Object.assign({}, tab, overrides);
  }

  const strings = {
    closeTab: 'Close tab',
    loadingTab: 'Loading...',
    tabCrashed: '$1 has crashed',
    tabNetworkError: '$1 has a network error',
  };

  setup(() => {
    loadTimeData.overrideValues(strings);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Set CSS variable for animations
    document.body.style.setProperty('--tabstrip-tab-height', '100px');
    document.body.style.setProperty('--tabstrip-tab-width', '280px');
    document.body.style.setProperty('--tabstrip-tab-spacing', '20px');

    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxyImpl.setInstance(testTabsApiProxy);

    tabElement = document.createElement('tabstrip-tab');
    tabElement.tab = createTabData();
    document.body.appendChild(tabElement);
  });

  test('slideOut animates out the element', async () => {
    testTabsApiProxy.setVisible(true);
    const tabElementStyle = window.getComputedStyle(tabElement);
    const animationPromise = tabElement.slideOut();
    // Before animation completes.
    assertEquals('1', tabElementStyle.opacity);
    assertEquals('none', tabElementStyle.maxWidth);
    assertEquals('matrix(1, 0, 0, 1, 0, 0)', tabElementStyle.transform);
    assertTrue(tabElement.isConnected);
    await animationPromise;
    // After animation completes.
    assertFalse(tabElement.isConnected);
  });

  test('slideOut does not animate when tab strip is hidden', () => {
    testTabsApiProxy.setVisible(false);
    assertTrue(tabElement.isConnected);
    tabElement.slideOut();

    // The tab should immediately be disconnected without waiting for the
    // animation to finish.
    assertFalse(tabElement.isConnected);
  });

  test('slideOut resolves immediately when tab strip becomes hidden', () => {
    testTabsApiProxy.setVisible(true);
    assertTrue(tabElement.isConnected);
    tabElement.slideOut();

    testTabsApiProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The tab should immediately be disconnected without waiting for the
    // animation to finish.
    assertFalse(tabElement.isConnected);
  });

  test('toggles an [active] attribute when active', () => {
    tabElement.tab = createTabData({active: true});
    assertTrue(tabElement.hasAttribute('active'));
    tabElement.tab = createTabData({active: false});
    assertFalse(tabElement.hasAttribute('active'));
  });

  test('sets [aria-selected] attribute when active', () => {
    tabElement.tab = createTabData({active: true});
    assertEquals('true', queryTab().getAttribute('aria-selected'));
    tabElement.tab = createTabData({active: false});
    assertEquals('false', queryTab().getAttribute('aria-selected'));
  });

  test('hides entire favicon container when showIcon is false', () => {
    // disable transitions
    tabElement.style.setProperty('--tabstrip-tab-transition-duration', '0ms');

    const faviconContainerStyle = window.getComputedStyle(
        tabElement.shadowRoot!.querySelector('#faviconContainer')!);

    tabElement.tab = createTabData({showIcon: true});
    assertEquals(
        faviconContainerStyle.maxWidth,
        faviconContainerStyle.getPropertyValue('--favicon-size').trim());
    assertEquals(faviconContainerStyle.opacity, '1');

    tabElement.tab = createTabData({showIcon: false});
    assertEquals(faviconContainerStyle.maxWidth, '0px');
    assertEquals(faviconContainerStyle.opacity, '0');
  });

  test('updates dimensions based on CSS variables when pinned', () => {
    const tabElementStyle = window.getComputedStyle(tabElement);
    const expectedSize = '100px';
    tabElement.style.setProperty('--tabstrip-pinned-tab-size', expectedSize);

    tabElement.tab = createTabData({pinned: true});
    assertEquals(expectedSize, tabElementStyle.width);
    assertEquals(expectedSize, tabElementStyle.height);

    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    tabElement.style.setProperty('--tabstrip-tab-height', '150px');
    tabElement.tab = createTabData({pinned: false});
    assertEquals('100px', tabElementStyle.width);
    assertEquals('150px', tabElementStyle.height);
  });

  test('show spinner when loading or waiting', () => {
    function assertSpinnerVisible(color: string) {
      const spinnerStyle = window.getComputedStyle(
          tabElement.shadowRoot!.querySelector('#progressSpinner')!);
      assertEquals('block', spinnerStyle.display);
      assertEquals(color, spinnerStyle.backgroundColor);

      // Also assert it becomes hidden when network state is NONE
      tabElement.tab = createTabData({networkState: TabNetworkState.kNone});
      assertEquals('none', spinnerStyle.display);
    }

    tabElement.style.setProperty(
        '--color-web-ui-tab-strip-tab-loading-spinning', 'rgb(255, 0, 0)');
    tabElement.tab = createTabData({networkState: TabNetworkState.kLoading});
    assertSpinnerVisible('rgb(255, 0, 0)');

    tabElement.style.setProperty(
        '--color-web-ui-tab-strip-tab-waiting-spinning', 'rgb(0, 255, 0)');
    tabElement.tab = createTabData({networkState: TabNetworkState.kWaiting});
    assertSpinnerVisible('rgb(0, 255, 0)');
  });

  test('shows blocked indicator when tab is blocked', () => {
    const blockIndicatorStyle = window.getComputedStyle(
        tabElement.shadowRoot!.querySelector('#blocked')!);
    tabElement.tab = createTabData({blocked: true});
    assertEquals('block', blockIndicatorStyle.display);
    tabElement.tab = createTabData({blocked: true, active: true});
    assertEquals('none', blockIndicatorStyle.display);
    tabElement.tab = createTabData({blocked: false});
    assertEquals('none', blockIndicatorStyle.display);
  });

  test(
      'hides the favicon and shows the crashed icon when tab is crashed',
      () => {
        // disable transitions
        tabElement.style.setProperty(
            '--tabstrip-tab-transition-duration', '0ms');

        const faviconStyle = window.getComputedStyle(
            tabElement.shadowRoot!.querySelector('#favicon')!);
        const crashedIconStyle = window.getComputedStyle(
            tabElement.shadowRoot!.querySelector('#crashedIcon')!);

        tabElement.tab = createTabData({crashed: true});
        assertEquals(faviconStyle.opacity, '0');
        assertEquals(crashedIconStyle.opacity, '1');

        tabElement.tab = createTabData({crashed: false});
        assertEquals(faviconStyle.opacity, '1');
        assertEquals(crashedIconStyle.opacity, '0');
      });

  test('clicking on the element activates the tab', async () => {
    queryTab().click();
    const tabId = await testTabsApiProxy.whenCalled('activateTab');
    assertEquals(tabId, tab.id);
  });

  test('ClickingElementClearsTouchPressedState', async () => {
    const touchPressedAttrib = 'touch_pressed_';
    tabElement.setTouchPressed(true);
    assertTrue(tabElement.hasAttribute(touchPressedAttrib));
    queryTab().click();
    assertFalse(tabElement.hasAttribute(touchPressedAttrib));
  });

  test('sets the title', () => {
    assertEquals(
        tab.title,
        tabElement.shadowRoot!.querySelector<HTMLElement>(
                                  '#titleText')!.innerText);
  });

  test('sets the loading title while loading', () => {
    const loadingTabWithoutTitle = createTabData({
      networkState: TabNetworkState.kWaiting,
      shouldHideThrobber: false,
    });
    delete (loadingTabWithoutTitle as Partial<Tab>).title;
    tabElement.tab = loadingTabWithoutTitle;
    assertEquals(
        strings['loadingTab'],
        tabElement.shadowRoot!.querySelector<HTMLElement>(
                                  '#titleText')!.innerText);
  });

  test('exposes the tab ID to an attribute', () => {
    tabElement.tab = createTabData({id: 1001});
    assertEquals('1001', tabElement.getAttribute('data-tab-id'));
  });

  test('closes the tab when clicking close button', async () => {
    tabElement.shadowRoot!.querySelector<HTMLElement>('#close')!.click();
    const [tabId, closeTabAction] =
        await testTabsApiProxy.whenCalled('closeTab');
    assertEquals(tabId, tab.id);
    assertEquals(closeTabAction, CloseTabAction.CLOSE_BUTTON);
  });

  test('closes the tab on swipe', async () => {
    tabElement.dispatchEvent(new CustomEvent('swipe'));
    const [tabId, closeTabAction] =
        await testTabsApiProxy.whenCalled('closeTab');
    assertEquals(tabId, tab.id);
    assertEquals(closeTabAction, CloseTabAction.SWIPED_TO_CLOSE);
  });

  test('sets the favicon to the favicon URL', () => {
    const expectedFaviconUrl = 'data:mock-favicon';
    tabElement.tab = createTabData({faviconUrl: {url: expectedFaviconUrl}});
    const faviconElement =
        tabElement.shadowRoot!.querySelector<HTMLElement>('#favicon')!;
    assertEquals(
        faviconElement.style.backgroundImage, `url("${expectedFaviconUrl}")`);
  });

  test(
      'sets the favicon to the default favicon URL if there is none provided',
      () => {
        const updatedTab = createTabData();
        updatedTab.faviconUrl = null;
        tabElement.tab = updatedTab;
        const faviconElement =
            tabElement.shadowRoot!.querySelector<HTMLElement>('#favicon')!;
        assertEquals(faviconElement.style.backgroundImage, getFavicon(''));
      });

  test('removes the favicon if the tab is waiting', () => {
    tabElement.tab = createTabData({
      faviconUrl: {url: 'data:mock-favicon'},
      networkState: TabNetworkState.kWaiting,
    });
    const faviconElement =
        tabElement.shadowRoot!.querySelector<HTMLElement>('#favicon')!;
    assertEquals(faviconElement.style.backgroundImage, 'none');
  });

  test(
      'removes the favicon if the tab is loading with a default favicon',
      () => {
        tabElement.tab = createTabData({
          faviconUrl: {url: 'data:mock-favicon'},
          isDefaultFavicon: true,
          networkState: TabNetworkState.kWaiting,
        });
        const faviconElement =
            tabElement.shadowRoot!.querySelector<HTMLElement>('#favicon')!;
        assertEquals(faviconElement.style.backgroundImage, 'none');
      });

  test('hides the thumbnail if there is no source yet', () => {
    const thumbnailImage =
        tabElement.shadowRoot!.querySelector<HTMLElement>('#thumbnailImg')!;
    assertFalse(thumbnailImage.hasAttribute('src'));
    assertEquals(window.getComputedStyle(thumbnailImage).display, 'none');
  });

  test('updates the thumbnail source', async () => {
    const thumbnailSource = 'data:mock-thumbnail-source';
    tabElement.updateThumbnail(thumbnailSource);
    assertEquals(
        tabElement.shadowRoot!.querySelector<HTMLImageElement>(
                                  '#thumbnailImg')!.src,
        thumbnailSource);
  });

  test('setting dragging state toggles an attribute', () => {
    tabElement.setDragging(true);
    assertTrue(tabElement.hasAttribute('dragging_'));
    tabElement.setDragging(false);
    assertFalse(tabElement.hasAttribute('dragging_'));
  });

  test('gets the drag image', () => {
    assertEquals(
        tabElement.getDragImage(),
        tabElement.shadowRoot!.querySelector('#dragImage'));
  });

  test('activating closes WebUI container', () => {
    assertEquals(testTabsApiProxy.getCallCount('closeContainer'), 0);
    queryTab().click();
    assertEquals(testTabsApiProxy.getCallCount('closeContainer'), 1);
  });

  test('sets an accessible title', () => {
    const titleTextElement =
        tabElement.shadowRoot!.querySelector<HTMLElement>('#titleText')!;
    assertEquals(titleTextElement.getAttribute('aria-label'), tab.title);

    tabElement.tab = createTabData({
      crashed: true,
      title: 'My tab',
    });
    assertEquals(
        titleTextElement.getAttribute('aria-label'), 'My tab has crashed');

    tabElement.tab = createTabData({
      crashed: false,
      networkState: TabNetworkState.kError,
      title: 'My tab',
    });
    assertEquals(
        titleTextElement.getAttribute('aria-label'),
        'My tab has a network error');
  });

  test('focusing on the host moves focus to inner tab element', () => {
    tabElement.focus();
    assertEquals(tabElement.shadowRoot!.activeElement, queryTab());
  });

  test('supports Enter and Space key for activating tab', async () => {
    const innerTabElement = queryTab();
    innerTabElement.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertEquals(await testTabsApiProxy.whenCalled('activateTab'), tab.id);
    testTabsApiProxy.reset();

    innerTabElement.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    assertEquals(await testTabsApiProxy.whenCalled('activateTab'), tab.id);
    testTabsApiProxy.reset();
  });

  test('DragImagePreservesAspectRatio', () => {
    const originalBoundingBox = queryTab().getBoundingClientRect();
    const originalAspectRatio =
        originalBoundingBox.width / originalBoundingBox.height;
    tabElement.setDragging(true);
    const dragImageBoundingBox =
        tabElement.getDragImage().querySelector<HTMLElement>(
                                     '#tab')!.getBoundingClientRect();
    const dragImageAspectRatio =
        dragImageBoundingBox.width / dragImageBoundingBox.height;
    // Check the Math.floor values of these values to prevent possible
    // flakiness caused by comparing float values.
    assertEquals(
        Math.floor(originalAspectRatio), Math.floor(dragImageAspectRatio));
  });

  test('RightClickOpensContextMenu', async () => {
    queryTab().dispatchEvent(new PointerEvent('pointerup', {
      pointerType: 'mouse',
      button: 2,
      clientX: 50,
      clientY: 100,
    }));
    const [id, x, y] = await testTabsApiProxy.whenCalled('showTabContextMenu');
    assertEquals(tab.id, id);
    assertEquals(50, x);
    assertEquals(100, y);
  });
});
