// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Cart} from 'chrome://new-tab-page/cart.mojom-webui.js';
import {CartTileModuleElementV2} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesHistoryClustersModuleCartTileV2Test', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function initializeModule(
      cart: Cart, showRelatedSearches: boolean = true,
      format: string = 'narrow'): Promise<CartTileModuleElementV2> {
    const tileElement = new CartTileModuleElementV2();
    tileElement.showRelatedSearches = showRelatedSearches;
    tileElement.cart = cart;
    tileElement.format = format;
    document.body.append(tileElement);
    await waitAfterNextRender(tileElement);
    return tileElement;
  }

  function createSampleCart(
      numImageUrls: number,
      overrides?: Partial<Cart>,
      ): Cart {
    const cart: Cart = Object.assign(
        {
          domain: 'foo.com',
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [...Array(numImageUrls).keys()].map((_, i) => {
            return {
              url: 'https://foo.com/image' + i,
            };
          }),
          discountText: '',
          relativeDate: '6 mins ago',
        },
        overrides);

    return cart;
  }

  test('Tile shows multiple images with extra image count card', async () => {
    // Arrange.
    const tileElement =
        await initializeModule(createSampleCart(5, {discountText: '5% off'}));

    // Assert.
    assertTrue(!!tileElement);
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('href'), 'https://foo.com');
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('aria-label'),
        loadTimeData.getStringF(
            'modulesJourneysCartTileLabelPlural', 5, '5% off', 'Foo', 'foo.com',
            '6 mins ago'));
    assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
    assertEquals(
        $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
        loadTimeData.getString('modulesJourneysCartAnnotation'));
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.small-image').length, 4);
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
    assertTrue(isVisible($$(tileElement, '#extraImageCard')!));
    assertEquals(
        $$(tileElement, '#extraImageCard')!.textContent!.replace(
            /[\r\n ]/gm, ''),
        '+2');
    assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
    assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
    assertTrue(isVisible($$(tileElement, '#discountChip')!));
    assertEquals($$(tileElement, '#discountChip')!.textContent!, '5% off');
  });

  test(
      'Tile shows multiple images without extra image count card', async () => {
        // Arrange.
        const tileElement = await initializeModule(createSampleCart(2));

        // Assert.
        assertTrue(!!tileElement);
        assertEquals(
            $$(tileElement, '#content')!.getAttribute('href'),
            'https://foo.com');
        assertEquals(
            $$(tileElement, '#content')!.getAttribute('aria-label'),
            loadTimeData.getStringF(
                'modulesJourneysCartTileLabelPlural', 2, '', 'Foo', 'foo.com',
                '6 mins ago'));
        assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
        assertTrue(isVisible($$(tileElement, '#titleAnnotation')!));
        assertEquals(
            $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
            loadTimeData.getString('modulesJourneysCartAnnotation'));
        assertEquals(
            tileElement.shadowRoot!.querySelectorAll('.small-image').length, 2);
        assertEquals(
            tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
        assertFalse(isVisible($$(tileElement, '#extraImageCard')!));
        assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
        assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
        assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
      });

  test('Tile shows single image', async () => {
    // Arrange.
    const tileElement = await initializeModule(createSampleCart(1));

    // Assert.
    assertTrue(!!tileElement);
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('href'), 'https://foo.com');
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('aria-label'),
        loadTimeData.getStringF(
            'modulesJourneysCartTileLabelSingular', '', 'Foo', 'foo.com',
            '6 mins ago'));
    assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
    assertTrue(isVisible($$(tileElement, '#titleAnnotation')!));
    assertEquals(
        $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
        loadTimeData.getString('modulesJourneysCartAnnotation'));
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.small-image').length, 0);
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.large-image').length, 1);
    assertFalse(isVisible($$(tileElement, '#extraImageCard')!));
    assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
    assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
  });

  test('Tile shows fallback image with favicon', async () => {
    // Arrange.
    const tileElement = await initializeModule(createSampleCart(0));

    // Assert.
    assertTrue(!!tileElement);
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('href'), 'https://foo.com');
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('aria-label'),
        loadTimeData.getStringF(
            'modulesJourneysCartTileLabelDefault', '', 'Foo', 'foo.com',
            '6 mins ago'));
    assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
    assertTrue(isVisible($$(tileElement, '#titleAnnotation')!));
    assertEquals(
        $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
        loadTimeData.getString('modulesJourneysCartAnnotation'));
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.small-image').length, 0);
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
    assertFalse(isVisible($$(tileElement, '#extraImageCard')!));
    assertTrue(isVisible($$(tileElement, '#fallbackImage')!));
    assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
  });

  test(
      'Tile does not render discount chip when discount is unavailable',
      async () => {
        // Arrange.
        const tileElement = await initializeModule(createSampleCart(0));

        // Assert.
        assertTrue(!!tileElement);
        assertTrue(!$$(tileElement, '#discountChip'));
      });

  test('Tile shows single image for not showing related searches', async () => {
    // Arrange.
    const tileElement = await initializeModule(createSampleCart(3), false);

    // Assert.
    assertTrue(!!tileElement);
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('href'), 'https://foo.com');
    assertEquals(
        $$(tileElement, '#content')!.getAttribute('aria-label'),
        loadTimeData.getStringF(
            'modulesJourneysCartTileLabelPlural', 3, '', 'Foo', 'foo.com',
            '6 mins ago'));
    assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
    assertTrue(isVisible($$(tileElement, '#titleAnnotation')!));
    assertEquals(
        $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
        loadTimeData.getString('modulesJourneysCartAnnotation'));
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.small-image').length, 2);
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
    assertTrue(isVisible($$(tileElement, '#extraImageCard')!));
    assertEquals(
        $$(tileElement, '#extraImageCard')!.textContent!.replace(
            /[\r\n ]/gm, ''),
        '+2');
    assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
    assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
  });

  test(
      'Tile shows single image for wide format eith no related searches',
      async () => {
        // Arrange.
        const tileElement =
            await initializeModule(createSampleCart(3), false, 'wide');

        // Assert.
        assertTrue(!!tileElement);
        assertEquals(
            $$(tileElement, '#content')!.getAttribute('href'),
            'https://foo.com');
        assertEquals(
            $$(tileElement, '#content')!.getAttribute('aria-label'),
            loadTimeData.getStringF(
                'modulesJourneysCartTileLabelPlural', 3, '', 'Foo', 'foo.com',
                '6 mins ago'));
        assertEquals($$(tileElement, '#title')!.textContent, 'Foo');
        assertTrue(isVisible($$(tileElement, '#titleAnnotation')!));
        assertEquals(
            $$(tileElement, '#titleAnnotation')!.textContent!.trim(),
            loadTimeData.getString('modulesJourneysCartAnnotation'));
        assertEquals(
            tileElement.shadowRoot!.querySelectorAll('.small-image').length, 2);
        assertEquals(
            tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
        assertTrue(isVisible($$(tileElement, '#extraImageCard')!));
        assertEquals(
            $$(tileElement, '#extraImageCard')!.textContent!.replace(
                /[\r\n ]/gm, ''),
            '+2');
        assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
        assertEquals($$(tileElement, '#label')!.textContent!, 'foo.com');
        assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
      });
});
