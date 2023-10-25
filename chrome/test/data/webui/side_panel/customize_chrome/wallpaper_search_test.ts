// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, Descriptors} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {DESCRIPTOR_D_VALUE, WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {$$, assertStyle, installMock} from './test_support.js';

suite('WallpaperSearchTest', () => {
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let wallpaperSearchElement: WallpaperSearchElement;

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
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
  });

  suite('Misc', () => {
    test('wallpaper search element added to side panel', () => {
      createWallpaperSearchElement();
      assertTrue(document.body.contains(wallpaperSearchElement));
    });

    test(
        'wallpaper search element hidden if there are no descriptors',
        async () => {
          createWallpaperSearchElement();
          await flushTasks();

          assertStyle(
              $$(wallpaperSearchElement, '.content')!, 'display', 'none');
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
          2,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorComboboxA .dropdown-item')
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
      assertDeepEquals(
          {color: hexColorToSkColor(DESCRIPTOR_D_VALUE[0]!)},
          handler.getArgs('getWallpaperSearchResults')[0][3]);
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
          createWallpaperSearchElement({
            descriptorA: [{category: 'foo', labels: ['bar', 'baz']}],
            descriptorB: [{label: 'foo', imagePath: 'bar.png'}],
            descriptorC: ['baz'],
          });
          await flushTasks();

          wallpaperSearchElement.$.submitButton.click();

          assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
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
  });
});
