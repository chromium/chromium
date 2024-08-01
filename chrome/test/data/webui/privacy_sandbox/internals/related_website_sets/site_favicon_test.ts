// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {SiteFaviconElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SiteFaviconTest', () => {
  let icon: SiteFaviconElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    icon = document.createElement('site-favicon');
    document.body.appendChild(icon);
  });

  test('on successful download', async () => {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/chrome_logo_dark.svg';
    await eventToPromise('site-favicon-loaded', icon);

    assertTrue(isVisible(icon.$.downloadedFavicon));
    assertFalse(isVisible(icon.$.favicon));
  });

  test('on failed download', async () => {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/invalid_url';
    await eventToPromise('site-favicon-error', icon);

    assertFalse(isVisible(icon.$.downloadedFavicon));
    assertTrue(isVisible(icon.$.favicon));
  });

  test('url change', async () => {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/chrome_logo_dark.svg';
    await eventToPromise('site-favicon-loaded', icon);

    assertTrue(isVisible(icon.$.downloadedFavicon));
    assertFalse(isVisible(icon.$.favicon));

    icon.url = '';
    await microtasksFinished();
    assertFalse(isVisible(icon.$.downloadedFavicon));
    assertTrue(isVisible(icon.$.favicon));
  });
});
