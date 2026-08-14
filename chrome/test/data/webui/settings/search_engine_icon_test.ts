// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsSearchEngineIconElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createSampleSearchEngine} from './test_search_engines_browser_proxy.js';

suite('SearchEngineIconTest', function() {
  let icon: SettingsSearchEngineIconElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    icon = document.createElement('settings-search-engine-icon');
    document.body.appendChild(icon);
  });

  // Test that when a search engine has an iconPath, site-favicon displays the
  // icon. Downloaded icon should not be visible.
  test('FaviconWithIconPath', async function() {
    icon.engine = createSampleSearchEngine({
      iconPath: 'images/foo.png',
      iconURL: 'http://www.google.com/favicon.ico',
    });
    await microtasksFinished();

    assertEquals(
        'chrome://image/?http://www.google.com/favicon.ico',
        icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has an iconURL and downloading is
  // successful, the downloaded icon is displayed. The site-favicon should not
  // be visible.
  test('FaviconWithIconURL_Successful', async function() {
    icon.engine = createSampleSearchEngine({
      iconPath: '',
      iconURL: 'https://example.com/icon.png',
    });
    await microtasksFinished();

    assertEquals(
        'chrome://image/?https://example.com/icon.png',
        icon.$.downloadedIcon.src);

    icon.$.downloadedIcon.dispatchEvent(new Event('load'));
    await icon.updateComplete;
    assertTrue(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertFalse(isVisible(favicon));
  });

  // Test that when a search engine has an iconURL and downloading fails,
  // site-favicon displays the icon.
  test('FaviconWithIconURL_Failed', async function() {
    icon.engine = createSampleSearchEngine(
        {iconPath: '', iconURL: 'https://example.com/invalid_url'});
    await microtasksFinished();

    assertEquals(
        'chrome://image/?https://example.com/invalid_url',
        icon.$.downloadedIcon.src);

    icon.$.downloadedIcon.dispatchEvent(new Event('error'));
    await microtasksFinished();
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has a data: iconURL, it is not passed to
  // cr-auto-img, and site-favicon displays the icon instead.
  test('FaviconWithDataURL', async function() {
    icon.engine = createSampleSearchEngine({
      iconPath: '',
      iconURL:
          'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJ' +
          'AAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==',
    });
    await microtasksFinished();

    assertEquals('', icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has a chrome:// iconURL, it is not passed to
  // cr-auto-img, and site-favicon displays the icon instead.
  test('FaviconWithChromeURL', async function() {
    icon.engine = createSampleSearchEngine({
      iconPath: '',
      iconURL: 'chrome://resources/images/chrome_logo_dark.svg',
    });
    await microtasksFinished();

    assertEquals('', icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has neither an iconPath nor an iconURL,
  // site-favicon displays the icon based on the search engine's URL.
  test('FaviconWithURL', async function() {
    icon.engine = createSampleSearchEngine({iconPath: '', iconURL: ''});
    await microtasksFinished();

    assertEquals('', icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon')!;
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Tests that the icon is not picked up by the screen reader.
  test('IconsHiddenForScreenReader', async function() {
    icon.engine = createSampleSearchEngine();
    await microtasksFinished();

    assertEquals('', icon.$.downloadedIcon.alt);
    const siteFavicon = icon.shadowRoot.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    assertEquals('true', siteFavicon.ariaHidden);
  });
});
