// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {WallpaperGridItem} from 'chrome://personalization/js/personalization_app.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('WallpaperGridItemTest', function() {
  let wallpaperGridItemElement: WallpaperGridItem|null;

  /**
   * Returns the match for |selector| in |wallpaperGridItemElement|'s shadow
   * DOM.
   */
  function querySelector<T extends Element>(selector: string) {
    return wallpaperGridItemElement!.shadowRoot!.querySelector<T>(selector);
  }

  teardown(async () => {
    await teardownElement(wallpaperGridItemElement);
    wallpaperGridItemElement = null;
  });

  test('displays empty state', async () => {
    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem);
    await waitAfterNextRender(wallpaperGridItemElement);

    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is set when no src is supplied');

    // Verify state.
    const img = querySelector('img');
    assertEquals(img?.getAttribute('auto-src'), null);
    assertEquals(img?.getAttribute('aria-hidden'), 'true');
    assertEquals(img?.hasAttribute('clear-src'), true);
    assertEquals(img?.hasAttribute('hidden'), true);
    assertEquals(img?.hasAttribute('is-google-photos'), true);
    assertEquals(querySelector('.text'), null);
    assertEquals(querySelector('.primary-text'), null);
    assertEquals(querySelector('.secondary-text'), null);
  });

  test('displays image', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    const img = querySelector('img');
    assertTrue(img!.hasAttribute('hidden'), 'image should be hidden at first');
    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute set while image is loading');
    await waitAfterNextRender(wallpaperGridItemElement);

    assertFalse(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute removed');

    // Verify state. Note that |img| is shown as |imageSrc| has already loaded.
    assertEquals(
        img?.getAttribute('auto-src'), src.url, 'auto-src set to correct url');
    assertEquals(
        img?.getAttribute('aria-hidden'), 'true', 'img is always aria-hidden');
    assertEquals(
        img?.hasAttribute('clear-src'), true, 'clear-src attribute always set');
    assertFalse(
        img!.hasAttribute('hidden'),
        'no longer hidden because image has loaded');
    assertEquals(
        img?.hasAttribute('is-google-photos'), true,
        'is-google-photos is always set');

    // Update state. Note that |img| is hidden as |imageSrc| hasn't yet loaded.
    const newSrc: Url = {url: src.url.replace('red', 'blue')};
    wallpaperGridItemElement.src = newSrc;
    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute set while new image is loading');
    assertEquals(img?.getAttribute('auto-src'), newSrc.url, 'new url is set');
    assertTrue(
        img!.hasAttribute('hidden'),
        'image should be hidden because src changed');
  });

  test('displays primary text', async () => {
    const primaryText = 'foo';

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {primaryText});
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertNotEquals(querySelector('.text'), null);
    assertEquals(querySelector('.primary-text')?.innerHTML, primaryText);
    assertEquals(querySelector('.secondary-text'), null);
  });

  test('displays secondary text', async () => {
    const secondaryText = 'foo';

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {secondaryText});
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertNotEquals(querySelector('.text'), null, 'text container is present');
    assertEquals(querySelector('.primary-text'), null, 'primary text is null');
    assertEquals(
        querySelector<HTMLParagraphElement>('.secondary-text')?.innerText,
        secondaryText, 'secondary text is correct string');
  });

  test('sets aria-selected based on selected property', async () => {
    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem);
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertEquals(
        wallpaperGridItemElement.ariaSelected, null,
        'aria selected attribute is initially missing');
    assertEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none',
        'iron-icon is display none when aria-selected is missing');

    // Select |wallpaperGridItemElement| and verify state.
    wallpaperGridItemElement.selected = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        wallpaperGridItemElement.ariaSelected, 'true',
        'aria selected attribute set to true when selected is true');
    assertNotEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none',
        'iron-icon is not display none when aria selected is true');

    // Deselect |wallpaperGridItemElement| and verify state.
    wallpaperGridItemElement.selected = false;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        wallpaperGridItemElement.ariaSelected, 'false',
        'aria selected back to false');
    assertEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none',
        'iron-icon is display none when aria selected is false');
  });
});
