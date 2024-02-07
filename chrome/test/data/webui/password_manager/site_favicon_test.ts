// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SiteFaviconElement} from 'chrome://password-manager/password_manager.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('SiteFaviconTest', function() {
  let icon: SiteFaviconElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    icon = document.createElement('site-favicon');
    document.body.appendChild(icon);
  });

  test('on successful download', async function() {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/chrome_logo_dark.svg';
    await eventToPromise('site-favicon-loaded', icon);

    assertTrue(isVisible(icon.$.downloadedFavicon));
    assertFalse(isVisible(icon.$.favicon));
  });

  test('on failed download', async function() {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/invalid_url';
    await eventToPromise('site-favicon-error', icon);

    assertFalse(isVisible(icon.$.downloadedFavicon));
    assertTrue(isVisible(icon.$.favicon));
  });

  test('url change', async function() {
    icon.domain = 'https://test.com';
    icon.url = 'chrome://resources/images/chrome_logo_dark.svg';
    await eventToPromise('site-favicon-loaded', icon);

    assertTrue(isVisible(icon.$.downloadedFavicon));
    assertFalse(isVisible(icon.$.favicon));

    icon.url = '';
    assertFalse(isVisible(icon.$.downloadedFavicon));
    assertTrue(isVisible(icon.$.favicon));
  });
});
