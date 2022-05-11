// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {WallpaperGridItem} from 'chrome://personalization/trusted/personalization_app.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('WallpaperGridItemTest', function() {
  let wallpaperGridItemElement: WallpaperGridItem|null;

  /**
   * Returns the match for |selector| in |wallpaperGridItemElement|'s shadow
   * DOM.
   */
  function querySelector(selector: string): Element|null {
    return wallpaperGridItemElement!.shadowRoot!.querySelector(selector);
  }

  teardown(async () => {
    await teardownElement(wallpaperGridItemElement);
    wallpaperGridItemElement = null;
  });

  test('displays empty state', async () => {
    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem);
    await waitAfterNextRender(wallpaperGridItemElement);

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
    let imageSrc = 'data:image/svg+xml;utf8,' +
        '<svg xmlns="http://www.w3.org/2000/svg" height="100px" width="100px">' +
        '<rect fill="red" height="100px" width="100px"></rect>' +
        '</svg>';

    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem, {imageSrc});
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state. Note that |img| is shown as |imageSrc| has already loaded.
    const img = querySelector('img');
    assertEquals(img?.getAttribute('auto-src'), imageSrc);
    assertEquals(img?.getAttribute('aria-hidden'), 'true');
    assertEquals(img?.hasAttribute('clear-src'), true);
    assertEquals(img?.hasAttribute('hidden'), false);
    assertEquals(img?.hasAttribute('is-google-photos'), true);

    // Update state. Note that |img| is hidden as |imageSrc| hasn't yet loaded.
    imageSrc = imageSrc.replace('red', 'blue');
    wallpaperGridItemElement.imageSrc = imageSrc;
    assertEquals(img?.getAttribute('auto-src'), imageSrc);
    assertEquals(img?.hasAttribute('hidden'), true);

    // Verify that once |imageSrc| has loaded, |img| will be shown.
    await new Promise<void>(resolve => {
      setInterval(() => {
        if (!img?.hasAttribute('hidden')) {
          resolve();
        }
      }, 100);
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
    assertNotEquals(querySelector('.text'), null);
    assertEquals(querySelector('.primary-text'), null);
    assertEquals(querySelector('.secondary-text')?.innerHTML, secondaryText);
  });

  test('displays selected', async () => {
    // Initialize |wallpaperGridItemElement|.
    wallpaperGridItemElement = initElement(WallpaperGridItem);
    await waitAfterNextRender(wallpaperGridItemElement);

    // Verify state.
    assertEquals(querySelector('.item[aria-selected]'), null);
    assertEquals(getComputedStyle(querySelector('iron-icon')!).display, 'none');

    // Select |wallpaperGridItemElement| and verify state.
    wallpaperGridItemElement.selected = true;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertNotEquals(querySelector('.item[aria-selected]'), null);
    assertNotEquals(
        getComputedStyle(querySelector('iron-icon')!).display, 'none');

    // Deselect |wallpaperGridItemElement| and verify state.
    wallpaperGridItemElement.selected = false;
    await waitAfterNextRender(wallpaperGridItemElement);
    assertEquals(querySelector('.item[aria-selected]'), null);
    assertEquals(getComputedStyle(querySelector('iron-icon')!).display, 'none');
  });
});
