// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {StackedFaviconsElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_URL_1 = 'https://google.com';
const TEST_URL_2 = 'https://youtube.com';
const TEST_URL_3 = 'https://chromium.org';
const TEST_URL_4 = 'https://maps.google.com';

const TEST_FAVICON_1 = getFaviconForPageURL(TEST_URL_1, false);
const TEST_FAVICON_2 = getFaviconForPageURL(TEST_URL_2, false);
const TEST_FAVICON_3 = getFaviconForPageURL(TEST_URL_3, false);
const TEST_FAVICON_4 = getFaviconForPageURL(TEST_URL_4, false);

suite('StackedFaviconsTest', () => {
  let element: StackedFaviconsElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('stacked-favicons');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('renders two favicons', async () => {
    element.url = TEST_URL_1;
    element.secondaryUrl = TEST_URL_2;
    await microtasksFinished();

    assertTrue(!!element.$.firstFavicon);
    assertTrue(!!element.$.secondFavicon);
    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_2, element.$.secondFavicon.style.backgroundImage);
  });

  test('updates favicons when URLs change', async () => {
    element.url = TEST_URL_1;
    element.secondaryUrl = TEST_URL_2;
    await microtasksFinished();

    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_2, element.$.secondFavicon.style.backgroundImage);

    element.url = TEST_URL_3;
    element.secondaryUrl = TEST_URL_4;
    await microtasksFinished();

    assertEquals(TEST_FAVICON_3, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_4, element.$.secondFavicon.style.backgroundImage);
  });

  test('default stacking orientation is horizontal', () => {
    assertFalse(element.stackVertically);
    assertFalse(element.hasAttribute('stack-vertically'));
  });

  test('reflects stack-vertically attribute', async () => {
    element.stackVertically = true;
    await microtasksFinished();

    assertTrue(element.stackVertically);
    assertTrue(element.hasAttribute('stack-vertically'));
  });
});
