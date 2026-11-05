// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import {isNtpUrl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerFaviconElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_URL_1 = 'https://google.com';
const TEST_URL_2 = 'https://youtube.com';
const TEST_URL_3 = 'https://chromium.org';
const TEST_URL_4 = 'https://maps.google.com';

const TEST_FAVICON_1 = getFaviconForPageURL(TEST_URL_1, false);
const TEST_FAVICON_2 = getFaviconForPageURL(TEST_URL_2, false);
const TEST_FAVICON_3 = getFaviconForPageURL(TEST_URL_3, false);
const TEST_FAVICON_4 = getFaviconForPageURL(TEST_URL_4, false);

suite('OrganizerFaviconTest', () => {
  let element: OrganizerFaviconElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('organizer-favicon');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('renders single favicon without clipping', async () => {
    element.url = TEST_URL_1;
    await microtasksFinished();

    assertTrue(!!element.$.firstFavicon);
    assertEquals(null, element.shadowRoot.querySelector('#secondFavicon'));
    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);
    assertTrue(hasStyle(element.$.firstFavicon, 'clip-path', 'none'));
  });

  test('renders two favicons', async () => {
    element.url = TEST_URL_1;
    element.secondaryUrl = TEST_URL_2;
    await microtasksFinished();

    const secondFavicon =
        element.shadowRoot.querySelector<HTMLElement>('#secondFavicon');
    assertTrue(!!element.$.firstFavicon);
    assertTrue(!!secondFavicon);
    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_2, secondFavicon.style.backgroundImage);
    assertFalse(hasStyle(element.$.firstFavicon, 'clip-path', 'none'));
  });

  test('updates favicons when URLs change', async () => {
    element.url = TEST_URL_1;
    element.secondaryUrl = TEST_URL_2;
    await microtasksFinished();

    const secondFavicon =
        element.shadowRoot.querySelector<HTMLElement>('#secondFavicon');
    assertTrue(!!secondFavicon);
    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_2, secondFavicon.style.backgroundImage);

    element.url = TEST_URL_3;
    element.secondaryUrl = TEST_URL_4;
    await microtasksFinished();

    assertEquals(TEST_FAVICON_3, element.$.firstFavicon.style.backgroundImage);
    assertEquals(TEST_FAVICON_4, secondFavicon.style.backgroundImage);
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

  test('renders NTP favicon with mask-image and foreground color', async () => {
    const ntpUrl = 'chrome://new-tab-page/';
    element.style.setProperty(
        '--color-organizer-panel-foreground', 'rgb(10, 20, 30)');
    element.url = ntpUrl;
    element.secondaryUrl = TEST_URL_2;
    await microtasksFinished();

    const secondFavicon =
        element.shadowRoot.querySelector<HTMLElement>('#secondFavicon');
    assertTrue(!!secondFavicon);
    assertTrue(element.$.firstFavicon.classList.contains('ntp'));
    assertEquals(
        getFaviconForPageURL(ntpUrl, false),
        element.$.firstFavicon.style.maskImage);
    assertEquals('', element.$.firstFavicon.style.backgroundImage);
    assertTrue(hasStyle(
        element.$.firstFavicon, 'background-color', 'rgb(10, 20, 30)'));

    assertFalse(secondFavicon.classList.contains('ntp'));
    assertEquals('', secondFavicon.style.maskImage);
    assertEquals(TEST_FAVICON_2, secondFavicon.style.backgroundImage);

    // Transition firstFavicon to regular URL and secondFavicon to NTP URL.
    const splitNtpUrl =
        'chrome://tab-search.top-chrome/split_new_tab_page.html';
    element.url = TEST_URL_1;
    element.secondaryUrl = splitNtpUrl;
    await microtasksFinished();

    assertFalse(element.$.firstFavicon.classList.contains('ntp'));
    assertEquals('', element.$.firstFavicon.style.maskImage);
    assertEquals(TEST_FAVICON_1, element.$.firstFavicon.style.backgroundImage);

    assertTrue(secondFavicon.classList.contains('ntp'));
    assertEquals(
        getFaviconForPageURL(splitNtpUrl, false),
        secondFavicon.style.maskImage);
    assertEquals('', secondFavicon.style.backgroundImage);
    assertTrue(hasStyle(secondFavicon, 'background-color', 'rgb(10, 20, 30)'));
  });

  test('identifies NTP URLs accurately', () => {
    assertTrue(isNtpUrl('chrome://newtab/'));
    assertTrue(isNtpUrl('chrome://new-tab-page/'));
    assertTrue(
        isNtpUrl('chrome://tab-search.top-chrome/split_new_tab_page.html'));

    assertFalse(isNtpUrl(''));
    assertFalse(isNtpUrl('not-a-valid-url'));
    assertFalse(isNtpUrl('https://newtab/'));
    assertFalse(isNtpUrl('chrome://version/'));
    assertFalse(isNtpUrl('chrome://new-tab-page-third-party/'));
  });
});
