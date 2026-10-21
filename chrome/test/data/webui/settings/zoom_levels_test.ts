// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {ZoomLevelEntry, ZoomLevelsElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

/** @fileoverview Suite of tests for zoom-levels. */

/**
 * An example zoom list.
 */
function createZoomList(): ZoomLevelEntry[] {
  return [
    {
      hostOrSpec: 'www.google.com',
      originForFavicon: 'www.google.com',
      displayName: 'www.google.com',
      zoom: '125%',
    },
    {
      hostOrSpec:
          'isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic',
      originForFavicon:
          'isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic',
      displayName: 'IWA Name',
      zoom: '125%',
    },
  ];
}

/**
 * Fetch the remove button from the list.
 */
function getRemoveButton(
    listContainer: HTMLElement, index: number): HTMLElement {
  const item = listContainer.children[index];
  assertTrue(!!item);
  const button = item.querySelector('cr-icon-button');
  assertTrue(!!button);
  return button;
}

suite('ZoomLevels', function() {
  /**
   * A zoom levels category created before each test.
   */
  let testElement: ZoomLevelsElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    return initPage();
  });

  async function initPage() {
    browserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('zoom-levels');
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('fetchZoomLevels');
    await microtasksFinished();
  }

  // Test that the tab title is updated when the subpage becomes active.
  test('UpdatesTitle', async function() {
    testElement.setAttribute(
        'route-path', routes.SITE_SETTINGS_ZOOM_LEVELS.path);
    await microtasksFinished();
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ZOOM_LEVELS);
    await microtasksFinished();
    assertEquals(
        loadTimeData.getStringF(
            'settingsAltPageTitle',
            loadTimeData.getString('siteSettingsCategoryZoomLevels')),
        document.title);
  });

  test('focusgroup attribute', function() {
    assertEquals(
        'listbox block', testElement.$.list.getAttribute('focusgroup'));
  });

  test('empty zoom state', function() {
    assertEquals(
        0, testElement.shadowRoot.querySelectorAll('.list-item').length);
    assertTrue(isVisible(testElement.$.empty));
  });

  test('non-empty zoom state', async function() {
    browserProxy.setZoomList(createZoomList());

    await initPage();

    assertFalse(isVisible(testElement.$.empty));
    assertEquals(
        2, testElement.shadowRoot.querySelectorAll('.list-item').length);

    const removeButton = getRemoveButton(testElement.$.list, 0);
    removeButton.click();
    const args = await browserProxy.whenCalled('removeZoomLevel');
    assertEquals('www.google.com', args[0]);
  });
});

suite('ZoomLevelsFocus', function() {
  let testElement: ZoomLevelsElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(async function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('zoom-levels');
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('fetchZoomLevels');
    await microtasksFinished();
  });

  test('focus restoration on zoom levels changed', async function() {
    const zoomList = createZoomList();
    const entry3: ZoomLevelEntry = {
      hostOrSpec: 'www.example.com',
      originForFavicon: 'www.example.com',
      displayName: 'www.example.com',
      zoom: '150%',
    };
    webUIListenerCallback('onZoomLevelsChanged', [...zoomList, entry3]);
    await microtasksFinished();
    assertEquals(
        3, testElement.shadowRoot.querySelectorAll('.list-item').length);

    // Case 1: Delete last item while focused.
    let removeButton = getRemoveButton(testElement.$.list, 2);
    removeButton.focus();
    assertEquals(removeButton, testElement.shadowRoot.activeElement);
    webUIListenerCallback('onZoomLevelsChanged', [...zoomList]);
    await microtasksFinished();

    // Verify focus moved to the new last item's remove button.
    assertEquals(
        2, testElement.shadowRoot.querySelectorAll('.list-item').length);
    assertEquals(
        getRemoveButton(testElement.$.list, 1),
        testElement.shadowRoot.activeElement);

    // Case 2: Delete first item while focused.
    removeButton = getRemoveButton(testElement.$.list, 0);
    removeButton.focus();
    assertEquals(removeButton, testElement.shadowRoot.activeElement);
    webUIListenerCallback('onZoomLevelsChanged', [zoomList[1]!]);
    await microtasksFinished();

    // Verify focus stayed on the item at index 0.
    assertEquals(
        1, testElement.shadowRoot.querySelectorAll('.list-item').length);
    assertEquals(
        getRemoveButton(testElement.$.list, 0),
        testElement.shadowRoot.activeElement);

    // Case 3: Delete last remaining item while focused.
    webUIListenerCallback('onZoomLevelsChanged', []);
    await microtasksFinished();
    assertEquals(
        0, testElement.shadowRoot.querySelectorAll('.list-item').length);
    assertNull(testElement.shadowRoot.activeElement);
  });
});
