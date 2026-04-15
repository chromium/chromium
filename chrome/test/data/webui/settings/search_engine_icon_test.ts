// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type { SettingsSearchEngineIconElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import { eventToPromise, isVisible } from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createSampleSearchEngine} from './test_search_engines_browser_proxy.js';


suite('SearchEngineIconTest', function() {
  let icon: SettingsSearchEngineIconElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    icon = document.createElement('settings-search-engine-icon');
    document.body.appendChild(icon);

    return flushTasks();
  });

  // Test that when a search engine has an iconPath, site-favicon displays the
  // icon. Downloaded icon should not be visible.
  test('FaviconWithIconPath', function() {
    icon.engine = createSampleSearchEngine({
      iconPath: 'images/foo.png',
      iconURL: 'http://www.google.com/favicon.ico',
    });

    assertEquals(
        'chrome://image/?http://www.google.com/favicon.ico',
        icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot!.querySelector('site-favicon');
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
      iconURL: 'chrome://resources/images/chrome_logo_dark.svg',
    });

    await eventToPromise('load', icon.$.downloadedIcon);
    assertEquals(
        'chrome://resources/images/chrome_logo_dark.svg',
        icon.$.downloadedIcon.src);
    assertTrue(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertFalse(isVisible(favicon));
  });

  // Test that when a search engine has an iconURL and downloading fails,
  // site-favicon displays the icon.
  test('FaviconWithIconURL_Failed', async function() {
    icon.engine = createSampleSearchEngine(
        {iconPath: '', iconURL: 'chrome://resources/images/invalid_url'});

    await eventToPromise('error', icon.$.downloadedIcon);
    assertEquals(
        'chrome://resources/images/invalid_url', icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has neither an iconPath nor an iconURL,
  // site-favicon displays the icon based on the search engine's URL.
  test('FaviconWithURL', function() {
    icon.engine = createSampleSearchEngine({iconPath: '', iconURL: ''});

    assertEquals('', icon.$.downloadedIcon.src);
    assertFalse(isVisible(icon.$.downloadedIcon));

    const siteFavicon = icon.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon')!;
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });
});
