// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, Descriptors} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {DESCRIPTOR_C_VALUE, WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle, installMock} from './test_support.js';

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

      assertNotStyle(
          $$(wallpaperSearchElement, '#descriptorBtnA')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#descriptorBtnB')!, 'display', 'none');
      assertNotStyle(
          $$(wallpaperSearchElement, '#descriptorBtnC')!, 'display', 'none');
      assertEquals(
          2,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorMenuA .dropdown-item')
              .length);
      assertEquals(
          1,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorMenuB .dropdown-item')
              .length);
      assertEquals(
          3,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorMenuC .dropdown-item')
              .length);
      assertEquals(
          5,
          wallpaperSearchElement.shadowRoot!
              .querySelectorAll('#descriptorMenuD cr-button')
              .length);
    });

    test('descriptor menus open and close', async () => {
      createWallpaperSearchElementWithDescriptors();
      await flushTasks();
      assertFalse(wallpaperSearchElement.$.descriptorMenuA.open);

      $$<HTMLElement>(wallpaperSearchElement, '#descriptorBtnA')!.click();

      assertTrue(wallpaperSearchElement.$.descriptorMenuA.open);

      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuA .dropdown-item')!.click();

      assertFalse(wallpaperSearchElement.$.descriptorMenuA.open);
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
          wallpaperSearchElement, '#descriptorMenuA .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuB .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuC .dropdown-item')!.click();
      $$<HTMLElement>(
          wallpaperSearchElement, '#descriptorMenuD cr-button')!.click();
      wallpaperSearchElement.$.submitButton.click();

      assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
      assertEquals('bar', handler.getArgs('getWallpaperSearchResults')[0][0]);
      assertEquals('foo', handler.getArgs('getWallpaperSearchResults')[0][1]);
      assertEquals('baz', handler.getArgs('getWallpaperSearchResults')[0][2]);
      assertEquals(
          DESCRIPTOR_C_VALUE[0],
          handler.getArgs('getWallpaperSearchResults')[0][3]);
    });

    test('selects random descriptor', async () => {
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
          wallpaperSearchElement, '#descriptorMenuA .dropdown-item')!.click();
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

      wallpaperSearchElement.$.submitButton.click();
      await waitAfterNextRender(wallpaperSearchElement);

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
