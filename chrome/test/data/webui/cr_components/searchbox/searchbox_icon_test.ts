// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {createAutocompleteMatch, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchboxIconElement} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

const HISTORY_URL: string = 'history-url';

function assertFaviconUrl(
    faviconImageUrl: URL, destinationUrl: string, scaleFactor: number,
    isTopChromeSearchbox: boolean) {
  assertEquals(faviconImageUrl.searchParams.get('size'), '16');
  assertEquals(
      faviconImageUrl.searchParams.get('scaleFactor'), `${scaleFactor}x`);
  assertEquals(faviconImageUrl.searchParams.get('pageUrl'), destinationUrl);
  assertEquals(
      faviconImageUrl.searchParams.get('allowGoogleServerFallback'), '0');
  // Top-chrome searchbox (i.e. WebUI Omnibox) should NOT force light-mode
  // icons in order to remain reactive to browser theme state (light/dark
  // mode).
  assertEquals(
      faviconImageUrl.searchParams.has('forceLightMode'),
      !isTopChromeSearchbox);
  assertEquals(
      faviconImageUrl.searchParams.get('forceEmptyDefaultFavicon'), '1');
}

suite('CrComponentsSearchboxIconTest', () => {
  let icon: SearchboxIconElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    icon = document.createElement('cr-searchbox-icon');
    icon.match = createAutocompleteMatch();
    document.body.appendChild(icon);
  });

  test('entity image shown on load', async () => {
    const match = createAutocompleteMatch();
    match.imageUrl = '#';
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
    assertTrue(!!image);
    const loadPromise = eventToPromise('load', image);
    image.dispatchEvent(new Event('load'));

    await loadPromise;

    assertTrue(isVisible(image));
  });

  test('entity image hidden on error', async () => {
    const match = createAutocompleteMatch();
    match.imageUrl = '#';
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
    assertTrue(!!image);
    const errorPromise = eventToPromise('error', image);
    image.dispatchEvent(new Event('error'));

    await errorPromise;

    assertFalse(isVisible(image));
  });

  for (const isTopChromeSearchbox of [true, false]) {
    test(
        `favicon image src and srcset are correct w/ isTopChromeSearchbox=${
            isTopChromeSearchbox}`,
        async () => {
          loadTimeData.overrideValues(
              {searchboxDefaultIcon: 'hello.svg', isTopChromeSearchbox});
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          icon = document.createElement('cr-searchbox-icon');
          document.body.appendChild(icon);

          const match = createAutocompleteMatch();
          match.isSearchType = false;
          match.type = HISTORY_URL;
          match.destinationUrl.url = 'http://www.fake-url-no-favicon.com/';
          icon.match = match;

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          // No favicon image should be rendered when the favicon service
          // doesn't find one for the given destination URL.
          assertFalse(isVisible(faviconImage));
          assertTrue(!!faviconImage);
          assertTrue(!!faviconImage.getAttribute('src'));

          const faviconImageUrl = new URL(faviconImage.getAttribute('src')!);
          assertFaviconUrl(
              faviconImageUrl, match.destinationUrl.url,
              /* scaleFactor= */ 1, isTopChromeSearchbox);

          const srcset = faviconImage.getAttribute('srcset');
          assertTrue(!!srcset);

          const faviconImageSrcSet = srcset.split(', ');
          assertEquals(faviconImageSrcSet.length, 2);
          for (let i = 0; i < faviconImageSrcSet.length; i++) {
            const [src, scaleFactor] = faviconImageSrcSet[i]!.split(' ');
            assertTrue(!!src);
            assertTrue(!!scaleFactor);
            assertFaviconUrl(
                new URL(src), match.destinationUrl.url,
                /* scaleFactor= */ i + 1, isTopChromeSearchbox);
            assertEquals(scaleFactor, `${i + 1}x`);
          }
        });
  }

  test('favicon image shown on load', async () => {
    const match = createAutocompleteMatch();
    match.isSearchType = false;
    match.type = HISTORY_URL;
    match.destinationUrl.url = 'http://www.example.com/';
    match.iconPath = 'globe.svg';
    icon.match = match;

    await microtasksFinished();

    const vectorIcon = icon.$.icon;
    assertTrue(!!vectorIcon);
    assertEquals(
        window.getComputedStyle(vectorIcon).webkitMaskImage,
        `url("chrome://new-tab-page/${match.iconPath}")`);

    const faviconImage = icon.$.faviconImage;
    assertTrue(!!faviconImage);
    const src = faviconImage.getAttribute('src');
    assertTrue(!!src);
    assertFaviconUrl(
        new URL(src), match.destinationUrl.url, /* scaleFactor= */ 1,
        /* isTopChromeSearchbox= */ false);

    assertTrue(isVisible(vectorIcon));
    assertFalse(isVisible(faviconImage));

    const loadPromise = eventToPromise('load', faviconImage);
    faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    assertFalse(isVisible(vectorIcon));
    assertTrue(isVisible(faviconImage));
  });

  test('favicon image hidden on error', async () => {
    const match = createAutocompleteMatch();
    match.isSearchType = false;
    match.type = HISTORY_URL;
    match.destinationUrl.url = 'http://www.example.com/';
    match.iconPath = 'globe.svg';
    icon.match = match;

    await microtasksFinished();

    const vectorIcon = icon.$.icon;
    assertTrue(!!vectorIcon);
    assertEquals(
        window.getComputedStyle(vectorIcon).webkitMaskImage,
        `url("chrome://new-tab-page/${match.iconPath}")`);

    const faviconImage = icon.$.faviconImage;
    assertTrue(!!faviconImage);
    const src = faviconImage.getAttribute('src');
    assertTrue(!!src);
    assertFaviconUrl(
        new URL(src), match.destinationUrl.url, /* scaleFactor= */ 1,
        /* isTopChromeSearchbox= */ false);

    assertTrue(isVisible(vectorIcon));
    assertFalse(isVisible(faviconImage));

    const errorPromise = eventToPromise('error', faviconImage);
    faviconImage.dispatchEvent(new Event('error'));
    await errorPromise;

    assertTrue(isVisible(vectorIcon));
    assertFalse(isVisible(faviconImage));
  });
});
