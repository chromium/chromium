// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {SiteSettingsPrefsBrowserProxyImpl, ZoomLevelEntry, ZoomLevelsElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
// clang-format on

/** @fileoverview Suite of tests for zoom-levels. */
suite('ZoomLevels', function() {
  /**
   * A zoom levels category created before each test.
   */
  let testElement: ZoomLevelsElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * An example zoom list.
   */
  const zoomList: ZoomLevelEntry[] = [
    {
      origin: 'http://www.google.com',
      displayName: 'http://www.google.com',
      originForFavicon: 'http://www.google.com',
      setting: '',
      source: '',
      zoom: '125%',
    },
    {
      origin: 'http://www.chromium.org',
      displayName: 'http://www.chromium.org',
      originForFavicon: 'http://www.chromium.org',
      setting: '',
      source: '',
      zoom: '125%',
    },
  ];

  setup(async function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    return initPage();
  });

  /** @return {!Promise} */
  function initPage() {
    browserProxy.reset();
    document.body.innerHTML = '';
    testElement = document.createElement('zoom-levels');
    document.body.appendChild(testElement);
    return browserProxy.whenCalled('fetchZoomLevels').then(() => {
      return waitBeforeNextRender(testElement);
    });
  }

  /**
   * Fetch the remove button from the list.
   * @param listContainer The list to use for the lookup.
   * @param index The index of the child element (which site) to
   *     fetch.
   */
  function getRemoveButton(listContainer: HTMLElement, index: number) {
    return listContainer.children[index]!.querySelector('cr-icon-button')!;
  }

  test('empty zoom state', function() {
    const list = testElement.$.list;
    assertTrue(!!list);
    assertEquals(0, list.items!.length);
    assertEquals(
        0, testElement.shadowRoot!.querySelectorAll('.list-item').length);
    assertFalse(testElement.$.empty.hidden);
  });

  test('non-empty zoom state', function() {
    browserProxy.setZoomList(zoomList);

    return initPage()
        .then(function() {
          const list = testElement.$.list;
          assertTrue(!!list);
          assertEquals(2, list.items!.length);
          assertTrue(testElement.$.empty.hidden);
          assertEquals(
              2, testElement.shadowRoot!.querySelectorAll('.list-item').length);

          const removeButton = getRemoveButton(testElement.$.listContainer, 0);
          assertTrue(!!removeButton);
          removeButton.click();
          return browserProxy.whenCalled('removeZoomLevel');
        })
        .then(function(args) {
          assertEquals('http://www.google.com', args[0]);
        });
  });
});
