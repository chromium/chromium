// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Cart} from 'chrome://new-tab-page/cart.mojom-webui.js';
import {CartTileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesHistoryClustersModuleCartTileTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function initializeModule(cart: Cart): Promise<CartTileModuleElement> {
    const tileElement = new CartTileModuleElement();
    tileElement.cart = cart;
    document.body.append(tileElement);
    await waitAfterNextRender(tileElement);
    return tileElement;
  }

  test('Tile shows multiple images with extra image count card', async () => {
    // Arrange.
    const cart = {
      domain: 'foo.com',
      merchant: 'Foo',
      cartUrl: {url: 'https://foo.com'},
      productImageUrls: [...Array(5).keys()].map((_, i) => {
        return {
          url: 'https://foo.com/image' + i,
        };
      }),
      discountText: '5% off',
      relativeDate: '6 mins ago',
    };
    const tileElement = await initializeModule(cart);

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
        tileElement.shadowRoot!.querySelectorAll('.small-image').length, 3);
    assertEquals(
        tileElement.shadowRoot!.querySelectorAll('.large-image').length, 0);
    assertTrue(isVisible($$(tileElement, '#extraImageCard')!));
    assertEquals($$(tileElement, '#extraImageCard')!.textContent!, '+2');
    assertFalse(isVisible($$(tileElement, '#fallbackImage')!));
    assertEquals($$(tileElement, '#domain')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
    assertTrue(isVisible($$(tileElement, '#discountChip')!));
    assertEquals($$(tileElement, '#discountChip')!.textContent!, '5% off');
  });

  test(
      'Tile shows multiple images without extra image count card', async () => {
        // Arrange.
        const cart = {
          domain: 'foo.com',
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [...Array(2).keys()].map((_, i) => {
            return {
              url: 'https://foo.com/image' + i,
            };
          }),
          discountText: '',
          relativeDate: '6 mins ago',
        };
        const tileElement = await initializeModule(cart);

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
        assertEquals($$(tileElement, '#domain')!.textContent!, 'foo.com');
        assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
      });

  test('Tile shows single image', async () => {
    // Arrange.
    const cart = {
      domain: 'foo.com',
      merchant: 'Foo',
      cartUrl: {url: 'https://foo.com'},
      productImageUrls: [...Array(1).keys()].map((_, i) => {
        return {
          url: 'https://foo.com/image' + i,
        };
      }),
      discountText: '',
      relativeDate: '6 mins ago',
    };
    const tileElement = await initializeModule(cart);

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
    assertEquals($$(tileElement, '#domain')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
  });

  test('Tile shows fallback image with favicon', async () => {
    // Arrange.
    const cart = {
      domain: 'foo.com',
      merchant: 'Foo',
      cartUrl: {url: 'https://foo.com'},
      productImageUrls: [],
      discountText: '',
      relativeDate: '6 mins ago',
    };
    const tileElement = await initializeModule(cart);

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
    assertEquals($$(tileElement, '#domain')!.textContent!, 'foo.com');
    assertEquals($$(tileElement, '#date')!.textContent!, '6 mins ago');
  });

  test(
      'Tile does not render discount chip when discount is unavailable',
      async () => {
        // Arrange.
        const cart = {
          domain: 'foo.com',
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
          discountText: '',
          relativeDate: '6 mins ago',
        };
        const tileElement = await initializeModule(cart);

        // Assert.
        assertTrue(!!tileElement);
        assertTrue(!$$(tileElement, '#discountChip'));
      });
});
