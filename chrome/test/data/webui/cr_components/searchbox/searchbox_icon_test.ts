// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchboxIconElement} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteMatch} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
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
    loadTimeData.overrideValues({
      isLensSearchbox: false,
      isTopChromeSearchbox: false,
      searchboxDefaultIcon: '',
    });
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
    const loadPromise = eventToPromise('load', image);
    image.dispatchEvent(new Event('load'));

    await loadPromise;

    assertTrue(isVisible(image));
  });

  test('image and icon URLs are encoded', async () => {
    // Regression test for crbug.com/501729582.
    const match = createAutocompleteMatch();
    const unsafeUrl = 'https://example.com/image.png?a=b&c=d';
    match.imageUrl = unsafeUrl;
    match.iconUrl = unsafeUrl;
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
    assertEquals(
        image.getAttribute('src'),
        '//image?staticEncode=true&encodeType=webp&url=https%3A%2F%2Fexample.com%2Fimage.png%3Fa%3Db%26c%3Dd');

    const iconImg = icon.$.iconImg;
    assertEquals(
        iconImg.getAttribute('src'),
        '//image?staticEncode=true&encodeType=webp&url=https%3A%2F%2Fexample.com%2Fimage.png%3Fa%3Db%26c%3Dd');
  });

  test('entity image hidden on error', async () => {
    const match = createAutocompleteMatch();
    match.imageUrl = '#';
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
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
          match.destinationUrl = 'http://www.fake-url-no-favicon.com/';
          icon.match = match;

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          // No favicon image should be rendered when the favicon service
          // doesn't find one for the given destination URL.
          assertFalse(isVisible(faviconImage));
          assertTrue(!!faviconImage.getAttribute('src'));

          const faviconImageUrl = new URL(faviconImage.getAttribute('src')!);
          assertFaviconUrl(
              faviconImageUrl, match.destinationUrl,
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
                new URL(src), match.destinationUrl,
                /* scaleFactor= */ i + 1, isTopChromeSearchbox);
            assertEquals(scaleFactor, `${i + 1}x`);
          }
        });
  }

  test('favicon image shown on load', async () => {
    const match = createAutocompleteMatch();
    match.isSearchType = false;
    match.type = HISTORY_URL;
    match.destinationUrl = 'http://www.example.com/';
    match.iconPath = 'globe.svg';
    icon.match = match;

    await microtasksFinished();

    const vectorIcon = icon.$.icon;
    assertEquals(
        window.getComputedStyle(vectorIcon).webkitMaskImage,
        `url("chrome://new-tab-page/${match.iconPath}")`);

    const faviconImage = icon.$.faviconImage;
    const src = faviconImage.getAttribute('src');
    assertTrue(!!src);
    assertFaviconUrl(
        new URL(src), match.destinationUrl, /* scaleFactor= */ 1,
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
    match.destinationUrl = 'http://www.example.com/';
    match.iconPath = 'globe.svg';
    icon.match = match;

    await microtasksFinished();

    const vectorIcon = icon.$.icon;
    assertEquals(
        window.getComputedStyle(vectorIcon).webkitMaskImage,
        `url("chrome://new-tab-page/${match.iconPath}")`);

    const faviconImage = icon.$.faviconImage;
    const src = faviconImage.getAttribute('src');
    assertTrue(!!src);
    assertFaviconUrl(
        new URL(src), match.destinationUrl, /* scaleFactor= */ 1,
        /* isTopChromeSearchbox= */ false);

    assertTrue(isVisible(vectorIcon));
    assertFalse(isVisible(faviconImage));

    const errorPromise = eventToPromise('error', faviconImage);
    faviconImage.dispatchEvent(new Event('error'));
    await errorPromise;

    assertTrue(isVisible(vectorIcon));
    assertFalse(isVisible(faviconImage));
  });

  test('suppresses favicon image for starter pack match', async () => {
    const match = createAutocompleteMatch();
    match.isSearchType = false;
    match.type = 'starter-pack';
    match.destinationUrl = 'https://example.com/';
    match.iconPath = 'starter_pack.svg';
    icon.match = match;

    await microtasksFinished();
    assertTrue(isVisible(icon.$.icon));
    assertFalse(isVisible(icon.$.faviconImage));
  });

  test('suppresses favicon image for pedal match', async () => {
    const match = createAutocompleteMatch();
    match.isSearchType = false;
    match.type = 'pedal';
    match.iconPath = 'pedal.svg';
    icon.match = match;

    await microtasksFinished();
    assertTrue(isVisible(icon.$.icon));
    assertFalse(isVisible(icon.$.faviconImage));
  });

  test(
      'suppresses favicon image for navigation match in Lens searchbox',
      async () => {
        loadTimeData.overrideValues({isLensSearchbox: true});
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        icon = document.createElement('cr-searchbox-icon');
        document.body.appendChild(icon);

        const match = createAutocompleteMatch();
        match.isSearchType = false;
        match.type = HISTORY_URL;
        match.destinationUrl = 'https://example.com/';
        match.iconPath = 'globe.svg';
        icon.match = match;

        await microtasksFinished();
        assertTrue(isVisible(icon.$.icon));
        assertFalse(isVisible(icon.$.faviconImage));
      });

  suite('inSearchbox', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isLensSearchbox: false,
        searchboxDefaultIcon: 'search_loupe.svg',
        isTopChromeSearchbox: true,
      });
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      icon = document.createElement('cr-searchbox-icon');
      icon.defaultIcon = 'search_loupe.svg';
      icon.inSearchbox = true;
      document.body.appendChild(icon);
    });

    test('renders favicon for navigation match', async () => {
      const match = createAutocompleteMatch();
      match.isSearchType = false;
      match.type = HISTORY_URL;
      match.destinationUrl = 'http://www.example.com/';
      match.iconPath = 'globe.svg';
      icon.match = match;

      await microtasksFinished();

      const vectorIcon = icon.$.icon;
      const faviconImage = icon.$.faviconImage;
      // Before favicon loads, vector icon is displayed as fallback.
      assertTrue(isVisible(vectorIcon));
      assertFalse(isVisible(faviconImage));

      // After favicon loads, favicon is displayed.
      const loadPromise = eventToPromise('load', faviconImage);
      faviconImage.dispatchEvent(new Event('load'));
      await loadPromise;
      await icon.updateComplete;

      assertFalse(isVisible(vectorIcon));
      assertTrue(isVisible(faviconImage));

      // If favicon fails to load, falls back to vector mask icon.
      const errorPromise = eventToPromise('error', faviconImage);
      faviconImage.dispatchEvent(new Event('error'));
      await errorPromise;
      await icon.updateComplete;

      assertTrue(isVisible(vectorIcon));
      assertFalse(isVisible(faviconImage));
      assertEquals(
          window.getComputedStyle(vectorIcon).webkitMaskImage,
          `url("chrome://new-tab-page/${match.iconPath}")`);
    });

    test('renders mask icon for search match', async () => {
      const match = createAutocompleteMatch();
      match.isSearchType = true;
      match.type = 'search-what-you-typed';
      match.iconPath = 'search_cr23.svg';
      icon.match = match;

      await microtasksFinished();

      const vectorIcon = icon.$.icon;
      const faviconImage = icon.$.faviconImage;
      assertTrue(isVisible(vectorIcon));
      assertFalse(isVisible(faviconImage));
      assertEquals(
          window.getComputedStyle(vectorIcon).webkitMaskImage,
          `url("chrome://new-tab-page/${match.iconPath}")`);
    });

    test('renders themed icon for document match', async () => {
      const match = createAutocompleteMatch();
      match.isSearchType = false;
      match.type = 'document';
      match.iconPath =
          '//resources/cr_components/searchbox/icons/drive_docs.svg';
      icon.match = match;

      await microtasksFinished();

      const faviconImage = icon.$.faviconImage;
      assertEquals(faviconImage.getAttribute('src'), match.iconPath);

      const loadPromise = eventToPromise('load', faviconImage);
      faviconImage.dispatchEvent(new Event('load'));
      await loadPromise;
      await icon.updateComplete;

      assertTrue(isVisible(faviconImage));
    });

    test(
        'renders Google G when `defaultIcon` is `google_g_gradient`',
        async () => {
          icon.defaultIcon =
              '//resources/cr_components/searchbox/icons/google_g_gradient.svg';
          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          assertEquals(
              faviconImage.getAttribute('src'),
              '//resources/cr_components/searchbox/icons/' +
                  'google_g_gradient.svg');

          const loadPromise = eventToPromise('load', faviconImage);
          faviconImage.dispatchEvent(new Event('load'));
          await loadPromise;
          await icon.updateComplete;

          assertTrue(isVisible(faviconImage));
        });

    test(
        'renders Google G for search match when `defaultIcon` is ' +
            '`google_g_gradient`',
        async () => {
          icon.defaultIcon =
              '//resources/cr_components/searchbox/icons/google_g_gradient.svg';
          const match = createAutocompleteMatch();
          match.isSearchType = true;
          match.type = 'search-what-you-typed';
          match.iconPath = 'search_cr23.svg';
          icon.match = match;

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          assertEquals(
              faviconImage.getAttribute('src'),
              '//resources/cr_components/searchbox/icons/' +
                  'google_g_gradient.svg');

          const loadPromise = eventToPromise('load', faviconImage);
          faviconImage.dispatchEvent(new Event('load'));
          await loadPromise;
          await icon.updateComplete;

          assertTrue(isVisible(faviconImage));
        });

    test(
        'renders non-Google DSE favicon when `defaultIcon` is a favicon URL',
        async () => {
          loadTimeData.overrideValues({
            isTopChromeSearchbox: true,
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          icon = document.createElement('cr-searchbox-icon');
          icon.defaultIcon = 'chrome://favicon2/?iconUrl=' +
              'https%3A%2F%2Fduckduckgo.com%2Ffavicon.ico&size=16&' +
              'scaleFactor=1x&forceEmptyDefaultFavicon=1';
          icon.inSearchbox = true;
          document.body.appendChild(icon);

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          assertTrue(faviconImage.getAttribute('src')!.includes(
              'iconUrl=https%3A%2F%2Fduckduckgo.com%2Ffavicon.ico'));

          const loadPromise = eventToPromise('load', faviconImage);
          faviconImage.dispatchEvent(new Event('load'));
          await loadPromise;
          await icon.updateComplete;

          assertTrue(isVisible(faviconImage));
        });

    test(
        'renders non-Google DSE favicon for search match when ' +
            '`defaultIcon` is a favicon URL',
        async () => {
          loadTimeData.overrideValues({
            isTopChromeSearchbox: true,
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          icon = document.createElement('cr-searchbox-icon');
          icon.defaultIcon = 'chrome://favicon2/?iconUrl=' +
              'https%3A%2F%2Fduckduckgo.com%2Ffavicon.ico&size=16&' +
              'scaleFactor=1x&forceEmptyDefaultFavicon=1';
          icon.inSearchbox = true;
          document.body.appendChild(icon);

          const match = createAutocompleteMatch();
          match.isSearchType = true;
          match.type = 'search-what-you-typed';
          match.iconPath = 'search_cr23.svg';
          icon.match = match;

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          assertTrue(faviconImage.getAttribute('src')!.includes(
              'iconUrl=https%3A%2F%2Fduckduckgo.com%2Ffavicon.ico'));

          const loadPromise = eventToPromise('load', faviconImage);
          faviconImage.dispatchEvent(new Event('load'));
          await loadPromise;
          await icon.updateComplete;

          assertTrue(isVisible(faviconImage));
        });

    test('renders page favicon when pageUrl is set without match', async () => {
      icon.defaultIcon =
          '//resources/cr_components/searchbox/icons/google_g_gradient.svg';
      icon.pageUrl = 'https://example.com';

      await microtasksFinished();

      const faviconImage = icon.$.faviconImage;
      assertTrue(faviconImage.getAttribute('src')!.includes(
          'pageUrl=https%3A%2F%2Fexample.com'));

      const loadPromise = eventToPromise('load', faviconImage);
      faviconImage.dispatchEvent(new Event('load'));
      await loadPromise;
      await icon.updateComplete;

      assertTrue(isVisible(faviconImage));

      // When favicon fails to load, falls back to generic globe
      // (page_cr23.svg).
      const errorPromise = eventToPromise('error', faviconImage);
      faviconImage.dispatchEvent(new Event('error'));
      await errorPromise;
      await icon.updateComplete;

      assertFalse(isVisible(faviconImage));
      assertTrue(window.getComputedStyle(icon.$.icon)
                     .webkitMaskImage.includes('page_cr23.svg'));
    });

    test(
        'renders keyword search loupe when inKeywordMode is set in searchbox',
        async () => {
          icon.defaultIcon =
              '//resources/cr_components/searchbox/icons/google_g_gradient.svg';
          icon.inSearchbox = true;
          icon.inKeywordMode = true;
          icon.pageUrl = 'https://example.com';
          icon.match = createAutocompleteMatch({
            isSearchType: false,
            destinationUrl: 'https://example.com',
          });

          await microtasksFinished();
          await icon.updateComplete;

          assertFalse(isVisible(icon.$.faviconImage));
          assertTrue(window.getComputedStyle(icon.$.icon)
                         .webkitMaskImage.includes('search_cr23.svg'));
        });

    test('reacts to `defaultIcon` property updates dynamically', async () => {
      loadTimeData.overrideValues({
        isTopChromeSearchbox: true,
      });
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      icon = document.createElement('cr-searchbox-icon');
      icon.defaultIcon =
          '//resources/cr_components/searchbox/icons/search_cr23.svg';
      icon.inSearchbox = true;
      document.body.appendChild(icon);

      await microtasksFinished();

      // Initially SVG mask icon, so mask image is used.
      assertFalse(isVisible(icon.$.faviconImage));
      assertTrue(isVisible(icon.$.icon));

      // Dynamically update defaultIcon to DSE favicon URL.
      icon.defaultIcon =
          'chrome://favicon2/?iconUrl=https%3A%2F%2Fbing.com%2Ffavicon.ico&' +
          'size=16&scaleFactor=1x&forceEmptyDefaultFavicon=1';
      await microtasksFinished();

      const faviconImage = icon.$.faviconImage;
      assertTrue(faviconImage.getAttribute('src')!.includes(
          'iconUrl=https%3A%2F%2Fbing.com%2Ffavicon.ico'));

      const loadPromise = eventToPromise('load', faviconImage);
      faviconImage.dispatchEvent(new Event('load'));
      await loadPromise;
      await icon.updateComplete;

      assertTrue(isVisible(faviconImage));
    });

    test(
        'falls back to search mask icon when `defaultIcon` favicon fails ' +
            'to load',
        async () => {
          loadTimeData.overrideValues({
            isTopChromeSearchbox: true,
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          icon = document.createElement('cr-searchbox-icon');
          icon.defaultIcon = 'chrome://favicon2/?iconUrl=' +
              'https%3A%2F%2Fduckduckgo.com%2Ffavicon.ico&size=16&' +
              'scaleFactor=1x&forceEmptyDefaultFavicon=1';
          icon.inSearchbox = true;
          document.body.appendChild(icon);

          await microtasksFinished();

          const faviconImage = icon.$.faviconImage;
          const vectorIcon = icon.$.icon;

          // Fail to load favicon.
          const errorPromise = eventToPromise('error', faviconImage);
          faviconImage.dispatchEvent(new Event('error'));
          await errorPromise;
          await icon.updateComplete;

          assertFalse(isVisible(faviconImage));
          assertTrue(isVisible(vectorIcon));
          assertTrue(window.getComputedStyle(vectorIcon)
                         .webkitMaskImage.includes('search_cr23.svg'));
        });
  });
});
