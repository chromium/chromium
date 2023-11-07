// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/wallpaper_search.js';
import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';

import {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {Descriptors, WallpaperSearchHandlerInterface, WallpaperSearchHandlerRemote, WallpaperSearchStatus} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.mojom-webui.js';
import {DESCRIPTOR_D_VALUE, WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/wallpaper_search.js';
import {WallpaperSearchProxy} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/wallpaper_search_proxy.js';
import {WindowProxy} from 'chrome://customize-chrome-side-panel.top-chrome/window_proxy.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, whenCheck} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle, createBackgroundImage, createTheme, installMock} from '../test_support.js';

suite('WallpaperSearchTest', () => {
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestMock<WallpaperSearchHandlerInterface>;
  let wallpaperSearchElement: WallpaperSearchElement;
  let windowProxy: TestMock<WindowProxy>;

  async function createWallpaperSearchElement(
      descriptors: Descriptors|null = null): Promise<WallpaperSearchElement> {
    handler.setResultFor('getDescriptors', Promise.resolve({descriptors}));
    wallpaperSearchElement =
        document.createElement('customize-chrome-wallpaper-search');
    document.body.appendChild(wallpaperSearchElement);
    return wallpaperSearchElement;
  }

  async function createWallpaperSearchElementWithDescriptors() {
    createWallpaperSearchElement({
      descriptorA: [{category: 'foo', labels: ['bar', 'baz']}],
      descriptorB: [{label: 'foo', imagePath: 'bar.png'}],
      descriptorC: ['foo', 'bar', 'baz'],
    });
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);
    handler = installMock(
        WallpaperSearchHandlerRemote,
        (mock: WallpaperSearchHandlerInterface) =>
            WallpaperSearchProxy.setHandler(mock));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  suite('Misc', () => {
    test('wallpaper search element added to side panel', () => {
      createWallpaperSearchElement();
      assertTrue(document.body.contains(wallpaperSearchElement));
    });

    test('clicking back button creates event', async () => {
      createWallpaperSearchElement();
      const eventPromise = eventToPromise('back-click', wallpaperSearchElement);
      wallpaperSearchElement.$.heading.getBackButton().click();
      const event = await eventPromise;
      assertTrue(!!event);
    });
  });

  suite('Descriptors', () => {
    test('descriptors are fetched from the backend', () => {
      createWallpaperSearchElement();
      assertEquals(1, handler.getCallCount('getDescriptors'));
    });

    test('descriptor menus populate correctly', async () => {
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      assertEquals(
          1,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxA .category-item')
              .length);
      assertEquals(
          1,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxB .dropdown-item')
              .length);
      assertEquals(
          3,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxC .dropdown-item')
              .length);
      assertEquals(
          6,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorMenuD cr-button')
              .length);
    });

    test('expands and collapses categories', async () => {
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      // No dropdown items by default since all categories are collapsed.
      assertEquals(
          0,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxA .dropdown-item')
              .length);

      const categoryLabel =
          wallpaperSearchElement.shadowRoot!.querySelector<HTMLElement>(
              '#descriptorComboboxA .category-item')!;
      const categoryLabelIcon = categoryLabel.querySelector('iron-icon')!;
      assertEquals('cr:expand-more', categoryLabelIcon.icon);

      // Clicking on a category expands the dropdown items below it.
      categoryLabel.click();
      await flushTasks();
      assertEquals(
          2,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxA .dropdown-item')
              .length);
      assertEquals('cr:expand-less', categoryLabelIcon.icon);

      // Clicking on the category again hides the dropdown items below it.
      categoryLabel.click();
      await flushTasks();
      assertEquals(
          0,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxA .dropdown-item')
              .length);
      assertEquals('cr:expand-more', categoryLabelIcon.icon);
    });

    test('check marks one item in descriptorMenuD at a time', async () => {
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      assertFalse(
          !!$$(wallpaperSearchElement, '#descriptorMenuD cr-button [checked]'));

      $$<HTMLElement>(wallpaperSearchElement, '.default-color')!.click();

      let checkedMarkedColors =
          wallpaperSearchElement.shadowRoot!.querySelectorAll(
              '#descriptorMenuD cr-button [checked]');
      assertEquals(1, checkedMarkedColors.length);
      assertEquals(
          checkedMarkedColors[0],
          $$(wallpaperSearchElement,
             '.default-color .color-check-mark-wrapper'));

      wallpaperSearchElement.$.hueSlider.dispatchEvent(
          new Event('selected-hue-changed'));

      checkedMarkedColors = wallpaperSearchElement.shadowRoot!.querySelectorAll(
          '#descriptorMenuD cr-button [checked]');
      assertEquals(1, checkedMarkedColors.length);
      assertEquals(
          checkedMarkedColors[0],
          $$(wallpaperSearchElement, '#customColorContainer [checked]'));
    });
  });

  suite('Search', () => {
    test('clicking search invokes backend', async () => {
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();

      assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
    });

    test('sends selected descriptor values to backend', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({results: ['123', '456']}));
      createWallpaperSearchElement({
        descriptorA: [{category: 'foo', labels: ['bar', 'baz']}],
        descriptorB: [{label: 'foo', imagePath: 'bar.png'}],
        descriptorC: ['baz'],
      });
      await flushTasks();

      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxA .category-item')!.click();
      await flushTasks();
      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxA .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxB .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxC .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuD cr-button')!.click();
      wallpaperSearchElement.$.submitButton.click();

      assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
      assertEquals('bar', handler.getArgs('getWallpaperSearchResults')[0][0]);
      assertEquals('foo', handler.getArgs('getWallpaperSearchResults')[0][1]);
      assertEquals('baz', handler.getArgs('getWallpaperSearchResults')[0][2]);
      const skColor = hexColorToSkColor(DESCRIPTOR_D_VALUE[0]!);
      assertNotEquals(skColor, {value: 0});
      assertDeepEquals(
          {color: skColor}, handler.getArgs('getWallpaperSearchResults')[0][3]);
    });

    test('sends hue to backend', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({results: ['123', '456']}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuD cr-button')!.click();

      wallpaperSearchElement.$.hueSlider.selectedHue = 10;
      wallpaperSearchElement.$.hueSlider.dispatchEvent(
          new Event('selected-hue-changed'));
      wallpaperSearchElement.$.submitButton.click();
      await flushTasks();

      assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
      assertDeepEquals(
          {hue: 10}, handler.getArgs('getWallpaperSearchResults')[0][3]);
    });

    test(
        'selects random descriptor a if user does not select one', async () => {
          handler.setResultFor(
              'getWallpaperSearchResults',
              Promise.resolve({results: ['123', '456']}));
          createWallpaperSearchElementWithDescriptors();
          await flushTasks();
          assertEquals(
              undefined, wallpaperSearchElement.$.descriptorComboboxA.value);

          wallpaperSearchElement.$.submitButton.click();

          assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
          assertNotEquals(
              undefined, wallpaperSearchElement.$.descriptorComboboxA.value);
          assertNotEquals(
              undefined, handler.getArgs('getWallpaperSearchResults')[0][0]);
        });

    test('sends one descriptor value to the backend', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults', Promise.resolve({results: []}));
      createWallpaperSearchElement({
        descriptorA: [{category: 'foo', labels: ['bar']}],
        descriptorB: [{label: 'foo', imagePath: 'bar.png'}],
        descriptorC: ['baz'],
      });
      await flushTasks();

      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxA .category-item')!.click();
      await flushTasks();
      $$<HTMLElement>(
          wallpaperSearchElement,
          '#descriptorComboboxA .dropdown-item')!.click();
      wallpaperSearchElement.$.submitButton.click();

      assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
      assertEquals('bar', handler.getArgs('getWallpaperSearchResults')[0][0]);
      assertEquals(
          undefined, handler.getArgs('getWallpaperSearchResults')[0][1]);
      assertEquals(
          undefined, handler.getArgs('getWallpaperSearchResults')[0][2]);
      assertEquals(
          undefined, handler.getArgs('getWallpaperSearchResults')[0][3]);
    });

    test('empty result shows no tiles', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults', Promise.resolve({results: []}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertTrue(!wallpaperSearchElement.shadowRoot!.querySelector('.tile'));
    });

    test('shows mix of filled and empty containers', async () => {
      handler.setResultFor('getWallpaperSearchResults', Promise.resolve({
        results: [
          {image: '123', id: {high: 10, low: 1}},
          {image: '456', id: {high: 8, low: 2}},
        ],
      }));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      // There should always be 6 tiles total. Since there are 2 images in the
      // response, there should be 2 result tiles and the remaining 4 should be
      // empty.
      assertEquals(
          wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile').length,
          6);
      assertEquals(
          wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile.result')
              .length,
          2);
      assertEquals(
          wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile.empty')
              .length,
          4);
    });

    test('handle result click', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({results: [{image: '123', id: {high: 10, low: 1}}]}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      assertFalse(isVisible(wallpaperSearchElement.$.loading));
      wallpaperSearchElement.$.submitButton.click();
      assertTrue(isVisible(wallpaperSearchElement.$.loading));
      await waitAfterNextRender(wallpaperSearchElement);
      assertFalse(isVisible(wallpaperSearchElement.$.loading));

      const result = $$(wallpaperSearchElement, '.tile.result');
      assertTrue(!!result);
      (result as HTMLElement).click();
      assertEquals(
          1, handler.getCallCount('setBackgroundToWallpaperSearchResult'));
      assertEquals(
          10, handler.getArgs('setBackgroundToWallpaperSearchResult')[0].high);
      assertEquals(
          1, handler.getArgs('setBackgroundToWallpaperSearchResult')[0].low);
    });

    test('results reset between search results', async () => {
      const exampleResults = {
        results: [{image: '123', id: {high: 10, low: 1}}],
      };
      handler.setResultFor(
          'getWallpaperSearchResults', Promise.resolve(exampleResults));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      // Check that there are tiles.
      let result = $$(wallpaperSearchElement, '.tile.result');
      assertTrue(!!result);

      // Create promise resolver so we have time between request and result to
      // test.
      const newResultsResolver = new PromiseResolver();
      handler.setResultFor(
          'getWallpaperSearchResults', newResultsResolver.promise);

      // Check that the previous tiles disappear after click until promise is
      // resolved, including the empty tiles.
      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);
      result = $$(wallpaperSearchElement, '.tile.result, .tile.empty');
      assertFalse(!!result);
      newResultsResolver.resolve(exampleResults);
      await waitAfterNextRender(wallpaperSearchElement);
      result = $$(wallpaperSearchElement, '.tile.result, .tile.empty');
      assertTrue(!!result);
    });

    test('sizes loading tiles', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({results: [{image: '123', id: {high: 10, low: 1}}]}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      // Force a width on the element for more consistent testing.
      wallpaperSearchElement.style.display = 'block';
      wallpaperSearchElement.style.width = '300px';

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      // Assert that the svg takes the full width of the content area.
      const svg =
          wallpaperSearchElement.$.loading.querySelector<HTMLElement>('svg')!;
      const contentWidth = wallpaperSearchElement.$.wallpaperSearch.offsetWidth;
      assertTrue(contentWidth < 300);
      await whenCheck(
          svg, () => svg.getAttribute('width') === `${contentWidth}`);

      // Assert that loading tiles are sized the same as result tiles.
      const resultTile =
          wallpaperSearchElement.shadowRoot!.querySelector<HTMLElement>(
              '.tile.result')!;
      const rects = wallpaperSearchElement.$.loading.querySelectorAll('rect');
      rects.forEach((rect) => {
        // Offset width/height values are automatically rounded, so round the
        // rect's dimensions. The difference in decimal pixel values is
        // negligible.
        assertEquals(
            resultTile.offsetWidth,
            Math.round(Number(rect.getAttribute('width'))));
        assertEquals(
            resultTile.offsetHeight,
            Math.round(Number(rect.getAttribute('height'))));
      });
    });

    test('handles changing submit button text', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({results: [{image: '123', id: {high: 10, low: 1}}]}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      // Check submit button text without results.
      assertEquals(wallpaperSearchElement.$.submitButton.innerText, 'Search');

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      // Check submit button text with results.
      assertEquals(
          wallpaperSearchElement.$.submitButton.innerText, 'Search Again');
    });

    test('current theme is checked', async () => {
      handler.setResultFor('getWallpaperSearchResults', Promise.resolve({
        results: [
          {image: '123', id: {high: BigInt(10), low: BigInt(1)}},
          {image: '456', id: {high: BigInt(8), low: BigInt(2)}},
        ],
      }));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      // Set a default theme.
      let theme = createTheme();
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      await waitAfterNextRender(wallpaperSearchElement);

      // Populate results.
      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      // There should be no checked tiles.
      assertFalse(!!$$(wallpaperSearchElement, '.tile [checked]'));

      // Set theme to the first tile.
      theme = createTheme();
      theme.backgroundImage = createBackgroundImage('');
      theme.backgroundImage.localBackgroundId = {
        high: BigInt(10),
        low: BigInt(1),
      };
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      await waitAfterNextRender(wallpaperSearchElement);

      // The first result should be checked and be the only one checked.
      const firstResult =
          $$(wallpaperSearchElement, '.tile .image-check-mark-wrapper');
      const checkedResults =
          wallpaperSearchElement.shadowRoot!.querySelectorAll(
              '.tile [checked]');
      assertEquals(checkedResults.length, 1);
      assertEquals(checkedResults[0], firstResult);
    });
  });

  suite('Error', () => {
    test(
        'shows error ui if no descriptors are returned by the backend',
        async () => {
          createWallpaperSearchElement(/*descriptors=*/ null);
          await flushTasks();

          wallpaperSearchElement.$.submitButton.click();
          await waitAfterNextRender(wallpaperSearchElement);

          assertNotStyle(
              $$(wallpaperSearchElement, '#error')!, 'display', 'none');
          assertStyle(
              $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display',
              'none');
        });

    test('reattempts failed descriptor fetch', async () => {
      createWallpaperSearchElement();
      await flushTasks();

      assertEquals(1, handler.getCallCount('getDescriptors'));
      assertNotStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');

      handler.setResultFor('getDescriptors', Promise.resolve({
        status: WallpaperSearchStatus.kOk,
        descriptors: {
          descriptorA: [{category: 'foo', labels: ['bar', 'baz']}],
          descriptorB: [{label: 'foo', imagePath: 'bar.png'}],
          descriptorC: ['foo', 'bar', 'baz'],
        },
      }));
      await flushTasks();

      $$<HTMLElement>(wallpaperSearchElement, '#errorCTA')!.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertEquals(2, handler.getCallCount('getDescriptors'));
      assertStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');
    });

    test('shows error ui if browser if offline', async () => {
      windowProxy.setResultFor('onLine', false);
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertEquals(1, windowProxy.getCallCount('onLine'));
      assertNotStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');
    });

    test('checks if browser is back online', async () => {
      windowProxy.setResultFor('onLine', false);
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertEquals(1, windowProxy.getCallCount('onLine'));
      windowProxy.setResultFor('onLine', true);

      $$<HTMLElement>(wallpaperSearchElement, '#errorCTA')!.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertEquals(2, windowProxy.getCallCount('onLine'));
      assertStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');
    });

    test('shows search ui if there are no errors', async () => {
      handler.setResultFor(
          'getWallpaperSearchResults',
          Promise.resolve({status: WallpaperSearchStatus.kOk, results: []}));
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();

      assertStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

      assertStyle($$(wallpaperSearchElement, '#error')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display', 'none');
    });

    [WallpaperSearchStatus.kError, WallpaperSearchStatus.kRequestThrottled]
        .forEach((status) => {
          test(
              `shows error ui if search fails with status of ${status}`,
              async () => {
                handler.setResultFor(
                    'getWallpaperSearchResults',
                    Promise.resolve({status: status, results: []}));
                createWallpaperSearchElementWithDescriptors();
                await flushTasks();

                wallpaperSearchElement.$.submitButton.click();
                await waitAfterNextRender(wallpaperSearchElement);

                assertNotStyle(
                    $$(wallpaperSearchElement, '#error')!, 'display', 'none');
                assertStyle(
                    $$(wallpaperSearchElement, '#wallpaperSearch')!, 'display',
                    'none');
              });
        });
  });
});
