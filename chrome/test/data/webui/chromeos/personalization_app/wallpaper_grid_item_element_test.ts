// Copyright 2022 The Chromium Authors
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

    assertEquals(
        querySelector('img'), null, 'no image is shown in empty state');
  });

  test('displays single image', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(images.length, 1, 'only one image is shown');
    const img = images[0];
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
  });

  test('forwards is-google-photos argument', async () => {
    const src: Url[] = [
      {url: createSvgDataUrl('0')},
      {url: createSvgDataUrl('1')},
    ];

    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    const images = wallpaperGridItemElement.shadowRoot?.querySelectorAll('img');
    assertEquals(images?.length, src.length, 'correct number of images shown');
    for (const image of images!) {
      assertFalse(
          image.hasAttribute('is-google-photos'), 'is-google-photos not set');
    }

    wallpaperGridItemElement.isGooglePhotos = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    const isGooglePhotosImages =
        wallpaperGridItemElement.shadowRoot?.querySelectorAll('img');
    assertEquals(
        isGooglePhotosImages?.length, src.length,
        'still correct number of images');
    for (const image of isGooglePhotosImages!) {
      assertTrue(
          image.hasAttribute('is-google-photos'), 'is-google-photos is set');
    }
  });

  test('updates to new image', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertFalse(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute removed when first image finished loading');

    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(images.length, 1, 'only one image is shown');
    const img = images[0];

    // Update state. Note that |img| is hidden as |imageSrc| hasn't yet loaded.
    const newSrc: Url = {url: src.url.replace('red', 'blue')};
    wallpaperGridItemElement.src = newSrc;
    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute set while new image is loading');
    assertTrue(
        img!.hasAttribute('hidden'),
        'image should be hidden because src changed');
    // dom-repeat takes another render to finalize setting new url.
    await waitAfterNextRender(
        wallpaperGridItemElement.shadowRoot!.querySelector('dom-repeat')!);
    assertEquals(img?.getAttribute('auto-src'), newSrc.url, 'new url is set');
  });

  test('does not set placeholder if new image src is identical', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};
    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    assertTrue(wallpaperGridItemElement.hasAttribute('placeholder'));
    await waitAfterNextRender(wallpaperGridItemElement);
    assertFalse(wallpaperGridItemElement.hasAttribute('placeholder'));

    const newSrc: Url = {url: src.url};
    wallpaperGridItemElement.src = newSrc;
    assertFalse(wallpaperGridItemElement.hasAttribute('placeholder'));
    await waitAfterNextRender(wallpaperGridItemElement);
    assertFalse(wallpaperGridItemElement.hasAttribute('placeholder'));
  });

  test('displays first two images', async () => {
    const src: Url[] = [
      {url: createSvgDataUrl('svg-0')},
      {url: createSvgDataUrl('svg-1')},
      {url: createSvgDataUrl('not-shown')},
    ];

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(images.length, 2, 'only first two images displayed');

    images.forEach((img, index) => {
      assertEquals(
          img.getAttribute('auto-src'), src[index]?.url,
          `url matches at index ${index}`);
    });
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

    // Select |wallpaperGridItemElement| while it is still loading in
    // placeholder state.
    wallpaperGridItemElement.selected = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        wallpaperGridItemElement.ariaSelected, 'true',
        'aria selected attribute set to true when selected is true');
    assertEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none',
        'iron-icon is still display none while image is loading');
    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is set');

    // Supply a src to make the image load.
    wallpaperGridItemElement.src = {url: createSvgDataUrl('test')};
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        wallpaperGridItemElement.ariaSelected, 'true',
        'aria selected attribute is still true');
    assertNotEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none',
        'iron-icon is not display none when aria selected is true');
    assertFalse(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is removed');

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
