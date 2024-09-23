// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('WallpaperGridItemElementTest', function() {
  let wallpaperGridItemElement: WallpaperGridItemElement|null;

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
    wallpaperGridItemElement = initElement(WallpaperGridItemElement);
    await waitAfterNextRender(wallpaperGridItemElement);

    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is set when no src is supplied');

    assertEquals(
        null, querySelector('img'), 'no image is shown in empty state');

    wallpaperGridItemElement.primaryText = 'cow';
    wallpaperGridItemElement.secondaryText = 'moo';
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(null, querySelector('#textShadow'), 'no text shadow shown');
    assertEquals(null, querySelector('#text'), 'no text shown');
  });

  test('displays single image', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(1, images.length, 'only one image is shown');
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
        src.url, img?.getAttribute('auto-src'), 'auto-src set to correct url');
    assertEquals(
        'true', img?.getAttribute('aria-hidden'), 'img is always aria-hidden');
    assertEquals(
        true, img?.hasAttribute('clear-src'), 'clear-src attribute always set');
    assertFalse(
        img!.hasAttribute('hidden'),
        'no longer hidden because image has loaded');
  });

  test('forwards is-google-photos argument', async () => {
    const src: Url[] = [
      {url: createSvgDataUrl('0')},
      {url: createSvgDataUrl('1')},
    ];

    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    const images = wallpaperGridItemElement.shadowRoot?.querySelectorAll('img');
    assertEquals(src.length, images?.length, 'correct number of images shown');
    for (const image of images!) {
      assertFalse(
          image.hasAttribute('is-google-photos'), 'is-google-photos not set');
    }

    wallpaperGridItemElement.isGooglePhotos = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    const isGooglePhotosImages =
        wallpaperGridItemElement.shadowRoot?.querySelectorAll('img');
    assertEquals(
        src.length, isGooglePhotosImages?.length,
        'still correct number of images');
    for (const image of isGooglePhotosImages!) {
      assertTrue(
          image.hasAttribute('is-google-photos'), 'is-google-photos is set');
    }
  });

  test('updates to new image', async () => {
    const src: Url = {url: createSvgDataUrl('svg-test')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertFalse(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute removed when first image finished loading');

    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(1, images.length, 'only one image is shown');
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
    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
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
    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    const images =
        wallpaperGridItemElement!.shadowRoot!.querySelectorAll('img');
    assertEquals(2, images.length, 'only first two images displayed');

    images.forEach((img, index) => {
      assertEquals(
          src[index]?.url, img.getAttribute('auto-src'),
          `url matches at index ${index}`);
    });
  });

  test('displays primary text', async () => {
    const primaryText = 'foo';
    const src = {url: createSvgDataUrl('0')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement =
        initElement(WallpaperGridItemElement, {primaryText, src});
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertNotEquals(
        null, querySelector('#textShadow'), 'text shadow is present');
    assertNotEquals(null, querySelector('#text'), '#text element exists');
    assertEquals(
        primaryText, querySelector('.primary-text')?.innerHTML,
        'primary text is correct');
    assertEquals(
        null, querySelector('.secondary-text'), 'secondary text not shown');
  });

  test('displays secondary text', async () => {
    const secondaryText = 'foo';
    const src = {url: createSvgDataUrl('0')};

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement =
        initElement(WallpaperGridItemElement, {secondaryText, src});
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertNotEquals(
        null, querySelector('#textShadow'), 'text shadow is present');
    assertNotEquals(null, querySelector('#text'), 'text container is present');
    assertEquals(null, querySelector('.primary-text'), 'primary text is null');
    assertEquals(
        secondaryText,
        querySelector<HTMLParagraphElement>('.secondary-text')?.innerText,
        'secondary text is correct string');
  });

  test('sets aria-selected based on selected property', async () => {
    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItemElement);
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertEquals(
        null, wallpaperGridItemElement.ariaSelected,
        'aria selected attribute is initially missing');
    assertEquals(
        'none', getComputedStyle(querySelector('iron-icon')!).display,
        'iron-icon is display none when aria-selected is missing');

    // Select |wallpaperGridItemElement| while it is still loading in
    // placeholder state.
    wallpaperGridItemElement.selected = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        'true', wallpaperGridItemElement.ariaSelected,
        'aria selected attribute set to true when selected is true');
    assertEquals(
        'none', getComputedStyle(querySelector('iron-icon')!).display,
        'iron-icon is still display none while image is loading');
    assertTrue(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is set');

    // Supply a src to make the image load.
    wallpaperGridItemElement.src = {url: createSvgDataUrl('test')};
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        'true', wallpaperGridItemElement.ariaSelected,
        'aria selected attribute is still true');
    assertNotEquals(
        'none', getComputedStyle(querySelector('iron-icon')!).display,
        'iron-icon is not display none when aria selected is true');
    assertFalse(
        wallpaperGridItemElement.hasAttribute('placeholder'),
        'placeholder attribute is removed');

    // Deselect |wallpaperGridItemElement| and verify state.
    wallpaperGridItemElement.selected = false;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        'false', wallpaperGridItemElement.ariaSelected,
        'aria selected back to false');
    assertEquals(
        'none', getComputedStyle(querySelector('iron-icon')!).display,
        'iron-icon is display none when aria selected is false');
  });

  test('sets aria-disabled attribute', async () => {
    wallpaperGridItemElement = initElement(WallpaperGridItemElement);
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        'false', wallpaperGridItemElement.getAttribute('aria-disabled'),
        'aria-disabled defaults to false');

    wallpaperGridItemElement.disabled = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        'true', wallpaperGridItemElement.getAttribute('aria-disabled'),
        'disabled sets aria-disabled attribute');

    wallpaperGridItemElement.disabled = false;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(
        'false', wallpaperGridItemElement.getAttribute('aria-disabled'),
        'disabled false sets aria-disabled attribute false');
  });

  test('collage shows up to four images', async () => {
    const src: Url[] =
        [0, 1, 2, 3, 4, 5].map(i => ({url: createSvgDataUrl(`${i}`)}));
    wallpaperGridItemElement = initElement(WallpaperGridItemElement, {src});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        2, wallpaperGridItemElement.shadowRoot!.querySelectorAll('img').length,
        'only 2 images shown by default');

    wallpaperGridItemElement.collage = true;
    await waitAfterNextRender(wallpaperGridItemElement);

    const images = Array.from(
        wallpaperGridItemElement.shadowRoot!.querySelectorAll('img'));
    assertEquals(4, images.length, 'collage shows 4 images');
    assertDeepEquals(
        src.slice(0, 4).map(({url}) => url), images.map(img => img.src),
        'first four image urls are shown');
  });

  test('shows an info icon for infoText', async () => {

    wallpaperGridItemElement = initElement(
        WallpaperGridItemElement,
        {infoText: 'some text', src: {url: createSvgDataUrl('test')}});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        'some text',
        wallpaperGridItemElement.shadowRoot!.getElementById('infoIcon')!.title,
        'icon exists and has title attribute');
  });

  test('no info icon if infoText is empty string', async () => {
    wallpaperGridItemElement = initElement(
        WallpaperGridItemElement,
        {infoText: '', src: {url: createSvgDataUrl('test')}});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertFalse(wallpaperGridItemElement.hasAttribute('placeholder'));

    assertEquals(
        null, wallpaperGridItemElement.shadowRoot!.getElementById('infoIcon'),
        'no info text set');

    wallpaperGridItemElement.infoText = 'description';
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        'description',
        wallpaperGridItemElement.shadowRoot!.getElementById('infoIcon')!.title,
        'correct title text now set');
  });

  test('no info icon if placeholder', async () => {
    wallpaperGridItemElement =
        initElement(WallpaperGridItemElement, {infoText: 'some text'});
    await waitAfterNextRender(wallpaperGridItemElement);

    assertTrue(wallpaperGridItemElement.hasAttribute('placeholder'));

    assertEquals(
        null, wallpaperGridItemElement.shadowRoot!.getElementById('infoIcon'),
        'no info text shown if placeholder');

    wallpaperGridItemElement.src = {url: createSvgDataUrl('testing')};
    await waitAfterNextRender(wallpaperGridItemElement);

    assertEquals(
        'some text',
        wallpaperGridItemElement.shadowRoot!.getElementById('infoIcon')!.title,
        'correct title text now set');
  });
});
