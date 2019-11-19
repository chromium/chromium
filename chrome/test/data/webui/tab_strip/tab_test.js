// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';

import {getFavicon} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {TabStripEmbedderProxy} from 'chrome://tab-strip/tab_strip_embedder_proxy.js';
import {TabNetworkState, TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestTabStripEmbedderProxy} from './test_tab_strip_embedder_proxy.js';
import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('Tab', function() {
  let testTabsApiProxy;
  let testTabStripEmbedderProxy;
  let tabElement;

  const tab = {
    active: false,
    alertStates: [],
    id: 1001,
    networkState: TabNetworkState.NONE,
    title: 'My title',
  };

  const strings = {
    closeTab: 'Close tab',
    loadingTab: 'Loading...',
    tabCrashed: '$1 has crashed',
    tabNetworkError: '$1 has a network error',
  };

  setup(() => {
    loadTimeData.overrideValues(strings);

    document.body.innerHTML = '';

    // Set CSS variable for animations
    document.body.style.setProperty('--tabstrip-tab-width', '280px');

    testTabStripEmbedderProxy = new TestTabStripEmbedderProxy();
    TabStripEmbedderProxy.instance_ = testTabStripEmbedderProxy;

    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxy.instance_ = testTabsApiProxy;

    tabElement = document.createElement('tabstrip-tab');
    tabElement.tab = tab;
    document.body.appendChild(tabElement);
  });

  test('slideIn animates in the element', async () => {
    document.documentElement.dir = 'ltr';
    tabElement.style.marginRight = '100px';
    const tabElementStyle = window.getComputedStyle(tabElement);

    const animationPromise = tabElement.slideIn();
    // Before animation completes.
    assertEquals('0px', tabElementStyle.marginRight);
    assertEquals('0px', tabElementStyle.maxWidth);
    assertEquals('matrix(0, 0, 0, 0, 0, 0)', tabElementStyle.transform);
    await animationPromise;
    // After animation completes.
    assertEquals('100px', tabElementStyle.marginRight);
    assertEquals('none', tabElementStyle.maxWidth);
    assertEquals('matrix(1, 0, 0, 1, 0, 0)', tabElementStyle.transform);
  });

  test('slideIn animations right to left for RTL languages', async () => {
    document.documentElement.dir = 'rtl';
    tabElement.style.marginLeft = '100px';
    const tabElementStyle = window.getComputedStyle(tabElement);

    const animationPromise = tabElement.slideIn();
    // Before animation completes.
    assertEquals('0px', tabElementStyle.marginLeft);
    assertEquals('0px', tabElementStyle.maxWidth);
    assertEquals('matrix(0, 0, 0, 0, 0, 0)', tabElementStyle.transform);
    await animationPromise;
    // After animation completes.
    assertEquals('100px', tabElementStyle.marginLeft);
    assertEquals('none', tabElementStyle.maxWidth);
    assertEquals('matrix(1, 0, 0, 1, 0, 0)', tabElementStyle.transform);
  });

  test('slideOut animates out the element', async () => {
    testTabStripEmbedderProxy.setVisible(true);
    const animationPromise = tabElement.slideOut();
    // Before animation completes.
    assertEquals('1', window.getComputedStyle(tabElement).opacity);
    assertEquals('none', window.getComputedStyle(tabElement).maxWidth);
    assertTrue(tabElement.isConnected);
    await animationPromise;
    // After animation completes.
    assertFalse(tabElement.isConnected);
  });

  test('slideOut does not animate when tab strip is hidden', () => {
    testTabStripEmbedderProxy.setVisible(false);
    assertTrue(tabElement.isConnected);
    tabElement.slideOut();

    // The tab should immediately be disconnected without waiting for the
    // animation to finish.
    assertFalse(tabElement.isConnected);
  });

  test('slideOut resolves immediately when tab strip becomes hidden', () => {
    testTabStripEmbedderProxy.setVisible(true);
    assertTrue(tabElement.isConnected);
    const animationPromise = tabElement.slideOut();

    testTabStripEmbedderProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The tab should immediately be disconnected without waiting for the
    // animation to finish.
    assertFalse(tabElement.isConnected);
  });

  test('toggles an [active] attribute when active', () => {
    tabElement.tab = Object.assign({}, tab, {active: true});
    assertTrue(tabElement.hasAttribute('active'));
    tabElement.tab = Object.assign({}, tab, {active: false});
    assertFalse(tabElement.hasAttribute('active'));
  });

  test('sets [aria-selected] attribute when active', () => {
    tabElement.tab = Object.assign({}, tab, {active: true});
    assertEquals(
        'true',
        tabElement.shadowRoot.querySelector('#tab').getAttribute(
            'aria-selected'));
    tabElement.tab = Object.assign({}, tab, {active: false});
    assertEquals(
        'false',
        tabElement.shadowRoot.querySelector('#tab').getAttribute(
            'aria-selected'));
  });

  test('hides entire favicon container when showIcon is false', () => {
    // disable transitions
    tabElement.style.setProperty('--tabstrip-tab-transition-duration', '0ms');

    const faviconContainerStyle = window.getComputedStyle(
        tabElement.shadowRoot.querySelector('#faviconContainer'));

    tabElement.tab = Object.assign({}, tab, {showIcon: true});
    assertEquals(
        faviconContainerStyle.maxWidth,
        faviconContainerStyle.getPropertyValue('--favicon-size').trim());
    assertEquals(faviconContainerStyle.opacity, '1');

    tabElement.tab = Object.assign({}, tab, {showIcon: false});
    assertEquals(faviconContainerStyle.maxWidth, '0px');
    assertEquals(faviconContainerStyle.opacity, '0');
  });

  test('updates dimensions based on CSS variables when pinned', () => {
    const tabElementStyle = window.getComputedStyle(tabElement);
    const expectedSize = '100px';
    tabElement.style.setProperty('--tabstrip-pinned-tab-size', expectedSize);

    tabElement.tab = Object.assign({}, tab, {pinned: true});
    assertEquals(expectedSize, tabElementStyle.width);
    assertEquals(expectedSize, tabElementStyle.height);

    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    tabElement.style.setProperty('--tabstrip-tab-height', '150px');
    tabElement.tab = Object.assign({}, tab, {pinned: false});
    assertEquals('100px', tabElementStyle.width);
    assertEquals('150px', tabElementStyle.height);
  });

  test('show spinner when loading or waiting', () => {
    function assertSpinnerVisible(color) {
      const spinnerStyle = window.getComputedStyle(
          tabElement.shadowRoot.querySelector('#progressSpinner'));
      assertEquals('block', spinnerStyle.display);
      assertEquals(color, spinnerStyle.backgroundColor);

      // Also assert it becomes hidden when network state is NONE
      tabElement.tab =
          Object.assign({}, tab, {networkState: TabNetworkState.NONE});
      assertEquals('none', spinnerStyle.display);
    }

    tabElement.style.setProperty(
        '--tabstrip-tab-loading-spinning-color', 'rgb(255, 0, 0)');
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.LOADING});
    assertSpinnerVisible('rgb(255, 0, 0)');

    tabElement.style.setProperty(
        '--tabstrip-tab-waiting-spinning-color', 'rgb(0, 255, 0)');
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.WAITING});
    assertSpinnerVisible('rgb(0, 255, 0)');
  });

  test('shows blocked indicator when tab is blocked', () => {
    const blockIndicatorStyle = window.getComputedStyle(
        tabElement.shadowRoot.querySelector('#blocked'));
    tabElement.tab = Object.assign({}, tab, {blocked: true});
    assertEquals('block', blockIndicatorStyle.display);
    tabElement.tab = Object.assign({}, tab, {blocked: true, active: true});
    assertEquals('none', blockIndicatorStyle.display);
    tabElement.tab = Object.assign({}, tab, {blocked: false});
    assertEquals('none', blockIndicatorStyle.display);
  });

  test(
      'hides the favicon and shows the crashed icon when tab is crashed',
      () => {
        // disable transitions
        tabElement.style.setProperty(
            '--tabstrip-tab-transition-duration', '0ms');

        const faviconStyle = window.getComputedStyle(
            tabElement.shadowRoot.querySelector('#favicon'));
        const crashedIconStyle = window.getComputedStyle(
            tabElement.shadowRoot.querySelector('#crashedIcon'));

        tabElement.tab = Object.assign({}, tab, {crashed: true});
        assertEquals(faviconStyle.opacity, '0');
        assertEquals(crashedIconStyle.opacity, '1');

        tabElement.tab = Object.assign({}, tab, {crashed: false});
        assertEquals(faviconStyle.opacity, '1');
        assertEquals(crashedIconStyle.opacity, '0');
      });

  test('clicking on the element activates the tab', () => {
    tabElement.shadowRoot.querySelector('#tab').click();
    return testTabsApiProxy.whenCalled('activateTab', tabId => {
      assertEquals(tabId, tab.id);
    });
  });

  test('sets the title', () => {
    assertEquals(
        tab.title, tabElement.shadowRoot.querySelector('#titleText').innerText);
  });

  test('sets the loading title while loading', () => {
    const loadingTabWithoutTitle = Object.assign({}, tab, {
      networkState: TabNetworkState.WAITING,
      shouldHideThrobber: false,
    });
    delete loadingTabWithoutTitle.title;
    tabElement.tab = loadingTabWithoutTitle;
    assertEquals(
        strings['loadingTab'],
        tabElement.shadowRoot.querySelector('#titleText').innerText);
  });

  test('exposes the tab ID to an attribute', () => {
    tabElement.tab = Object.assign({}, tab, {id: 1001});
    assertEquals('1001', tabElement.getAttribute('data-tab-id'));
  });

  test('closes the tab', () => {
    tabElement.shadowRoot.querySelector('#close').click();
    return testTabsApiProxy.whenCalled('closeTab').then(tabId => {
      assertEquals(tabId, tab.id);
    });
  });

  test('sets the favicon to the favicon URL', () => {
    const expectedFaviconUrl = 'data:mock-favicon';
    tabElement.tab = Object.assign({}, tab, {favIconUrl: expectedFaviconUrl});
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(
        faviconElement.style.backgroundImage, `url("${expectedFaviconUrl}")`);
  });

  test(
      'sets the favicon to the default favicon URL if there is none provided',
      () => {
        const updatedTab = Object.assign({}, tab);
        delete updatedTab.favIconUrl;
        tabElement.tab = updatedTab;
        const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
        assertEquals(faviconElement.style.backgroundImage, getFavicon(''));
      });

  test('removes the favicon if the tab is waiting', () => {
    tabElement.tab = Object.assign({}, tab, {
      favIconUrl: 'data:mock-favicon',
      networkState: TabNetworkState.WAITING,
    });
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(faviconElement.style.backgroundImage, 'none');
  });

  test(
      'removes the favicon if the tab is loading with a default favicon',
      () => {
        tabElement.tab = Object.assign({}, tab, {
          favIconUrl: 'data:mock-favicon',
          hasDefaultFavicon: true,
          networkState: TabNetworkState.WAITING,
        });
        const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
        assertEquals(faviconElement.style.backgroundImage, 'none');
      });

  test('hides the thumbnail if there is no source yet', () => {
    const thumbnailImage = tabElement.shadowRoot.querySelector('#thumbnailImg');
    assertFalse(thumbnailImage.hasAttribute('src'));
    assertEquals(window.getComputedStyle(thumbnailImage).display, 'none');
  });

  test('updates the thumbnail source', async () => {
    const thumbnailSource = 'data:mock-thumbnail-source';
    tabElement.updateThumbnail(thumbnailSource);
    assertEquals(
        tabElement.shadowRoot.querySelector('#thumbnailImg').src,
        thumbnailSource);
  });

  test('setting dragging state toggles an attribute', () => {
    tabElement.setDragging(true);
    assertTrue(tabElement.hasAttribute('dragging_'));
    tabElement.setDragging(false);
    assertFalse(tabElement.hasAttribute('dragging_'));
  });

  test('getting the drag image grabs the contents', () => {
    assertEquals(
        tabElement.getDragImage(), tabElement.shadowRoot.querySelector('#tab'));
  });

  test('has custom context menu', async () => {
    let event = new Event('contextmenu');
    event.clientX = 1;
    event.clientY = 2;
    tabElement.shadowRoot.querySelector('#tab').dispatchEvent(event);

    const contextMenuArgs =
        await testTabStripEmbedderProxy.whenCalled('showTabContextMenu');
    assertEquals(contextMenuArgs[0], tabElement.tab.id);
    assertEquals(contextMenuArgs[1], 1);
    assertEquals(contextMenuArgs[2], 2);
  });

  test('activating closes WebUI container', () => {
    assertEquals(testTabStripEmbedderProxy.getCallCount('closeContainer'), 0);
    tabElement.shadowRoot.querySelector('#tab').click();
    assertEquals(testTabStripEmbedderProxy.getCallCount('closeContainer'), 1);
  });

  test('sets an accessible title', () => {
    const titleTextElement = tabElement.shadowRoot.querySelector('#titleText');
    assertEquals(titleTextElement.getAttribute('aria-label'), tab.title);

    tabElement.tab = Object.assign({}, tab, {
      crashed: true,
      title: 'My tab',
    });
    assertEquals(
        titleTextElement.getAttribute('aria-label'), 'My tab has crashed');

    tabElement.tab = Object.assign({}, tab, {
      crashed: false,
      networkState: TabNetworkState.ERROR,
      title: 'My tab',
    });
    assertEquals(
        titleTextElement.getAttribute('aria-label'),
        'My tab has a network error');
  });

  test('focusing on the host moves focus to inner tab element', () => {
    tabElement.focus();
    assertEquals(
        tabElement.shadowRoot.activeElement,
        tabElement.shadowRoot.querySelector('#tab'));
  });

  test('supports Enter and Space key for activating tab', async () => {
    const innerTabElement = tabElement.shadowRoot.querySelector('#tab');
    innerTabElement.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertEquals(await testTabsApiProxy.whenCalled('activateTab'), tab.id);
    testTabsApiProxy.reset();

    innerTabElement.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    assertEquals(await testTabsApiProxy.whenCalled('activateTab'), tab.id);
    testTabsApiProxy.reset();
  });
});
