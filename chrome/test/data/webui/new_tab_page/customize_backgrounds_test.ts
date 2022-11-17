// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {CustomizeBackgroundsElement} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {BackgroundCollection, CollectionImage, PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

function createCollection(
    id: string = '0', label: string = '',
    url: string = ''): BackgroundCollection {
  return {id, label, previewImageUrl: {url}};
}

suite('NewTabPageCustomizeBackgroundsTest', () => {
  let windowProxy: TestBrowserProxy<WindowProxy>;
  let handler: TestBrowserProxy<PageHandlerRemote>;

  async function createCustomizeBackgrounds():
      Promise<CustomizeBackgroundsElement> {
    const customizeBackgrounds =
        document.createElement('ntp-customize-backgrounds');
    const theme = createTheme();
    theme.isCustomBackground = false;
    customizeBackgrounds.theme = theme;
    document.body.appendChild(customizeBackgrounds);
    await handler.whenCalled('getBackgroundCollections');
    await flushTasks();
    return customizeBackgrounds;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('createIframeSrc', '');

    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [],
    }));
  });

  test('creating element shows background collection tiles', async () => {
    // Arrange.
    const collection = createCollection('0', 'col_0', 'https://col_0.jpg');
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [collection],
    }));

    // Act.
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Assert.
    assertTrue(isVisible(customizeBackgrounds.$.collections));
    assertStyle(customizeBackgrounds.$.images, 'display', 'none');
    const tiles =
        customizeBackgrounds.shadowRoot!.querySelectorAll('#collections .tile');
    assertEquals(3, tiles.length);
    assertEquals('col_0', tiles[2]!.getAttribute('title'));
    assertEquals(
        'chrome-untrusted://new-tab-page/background_image?https://col_0.jpg',
        tiles[2]!.querySelector<HTMLImageElement>('.image')!.src);
  });

  test('clicking collection selects collection', async function() {
    // Arrange.
    const collection = createCollection();
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [collection],
    }));
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Act.
    customizeBackgrounds.shadowRoot!
        .querySelector<HTMLElement>('#collections .tile:nth-child(3)')!.click();

    // Assert.
    assertDeepEquals(collection, customizeBackgrounds.selectedCollection);
  });

  test('setting collection requests images', async function() {
    // Arrange.
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Act.
    customizeBackgrounds.selectedCollection = createCollection();

    // Assert.
    assertFalse(isVisible(customizeBackgrounds.$.collections));
    await handler.whenCalled('getBackgroundImages');
  });

  test('Loading images shows image tiles', async function() {
    // Arrange.
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://a.com/i.png'},
      previewImageUrl: {url: 'https://a.com/p.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    const customizeBackgrounds = await createCustomizeBackgrounds();
    customizeBackgrounds.selectedCollection = createCollection();

    // Act.
    const id = await handler.whenCalled('getBackgroundImages');
    await flushTasks();

    // Assert.
    assertEquals(id, '0');
    assertFalse(isVisible(customizeBackgrounds.$.collections));
    assertTrue(isVisible(customizeBackgrounds.$.images));
    const tiles =
        customizeBackgrounds.shadowRoot!.querySelectorAll('#images .tile');
    assertEquals(tiles.length, 1);
    assertEquals(
        tiles[0]!.querySelector<HTMLImageElement>('.image')!.src,
        'chrome-untrusted://new-tab-page/background_image?https://a.com/p.png');
  });

  test('Going back shows collections', async function() {
    // Arrange.
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://example.com/image.png'},
      previewImageUrl: {url: 'https://example.com/image.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();

    // Act.
    customizeBackgrounds.selectedCollection = null;
    await flushTasks();

    // Assert.
    assertNotStyle(customizeBackgrounds.$.collections, 'display', 'none');
    assertStyle(customizeBackgrounds.$.images, 'display', 'none');
  });

  test('select different image', async () => {
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://example.com/image.png'},
      previewImageUrl: {url: 'https://example.com/image.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.theme.isCustomBackground = true;
    customizeBackgrounds.theme.backgroundImage =
        createBackgroundImage('https://example.com/notthesameimage.png');
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();
    const element = customizeBackgrounds.shadowRoot!.querySelector<HTMLElement>(
        '#images .tile')!;
    const item = customizeBackgrounds.$.imagesRepeat.itemForElement(element);
    assertEquals(image.attribution1, item.attribution1);
    assertFalse(element.classList.contains('selected'));
    element.click();
    assertEquals(1, handler.getCallCount('setBackgroundImage'));
    assertEquals(1, handler.getCallCount('onCustomizeDialogAction'));
  });

  test('select same image', async () => {
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://example.com/image.png'},
      previewImageUrl: {url: 'https://example.com/image.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.theme.isCustomBackground = true;
    customizeBackgrounds.theme.backgroundImage =
        createBackgroundImage('https://example.com/image.png');
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();
    const element = customizeBackgrounds.shadowRoot!.querySelector<HTMLElement>(
        '#images .tile')!;
    element.click();
    assertEquals(1, handler.getCallCount('setBackgroundImage'));
    assertEquals(0, handler.getCallCount('onCustomizeDialogAction'));
  });

  test('image selected by current theme', async () => {
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://example.com/image.png'},
      previewImageUrl: {url: 'https://example.com/image.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    customizeBackgrounds.theme.backgroundImage =
        createBackgroundImage('https://example.com/image.png');
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();
    const element =
        customizeBackgrounds.shadowRoot!.querySelector('#images .tile')!;
    assertTrue(element.classList.contains('selected'));
  });

  test('deselected when background selection is not an image', async () => {
    const image: CollectionImage = {
      attribution1: 'image_0',
      imageUrl: {url: 'https://example.com/image.png'},
      previewImageUrl: {url: 'https://example.com/image.png'},
      attributionUrl: {url: ''},
      attribution2: '',
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();
    const element = customizeBackgrounds.shadowRoot!.querySelector<HTMLElement>(
        '#images .tile')!;
    element.click();
    assertEquals(1, handler.getCallCount('setBackgroundImage'));
  });

  test('choosing local dispatches cancel', async () => {
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor(
        'chooseLocalCustomBackground', Promise.resolve({success: true}));
    const waitForClose = eventToPromise('close', customizeBackgrounds);
    customizeBackgrounds.$.uploadFromDevice.click();
    await handler.whenCalled('chooseLocalCustomBackground');
    await waitForClose;
  });

  suite('no background', () => {
    let customizeBackgrounds: CustomizeBackgroundsElement;

    setup(async () => {
      customizeBackgrounds = await createCustomizeBackgrounds();
    });

    function assertSetNoBackgroundImageNotCalled() {
      assertEquals(0, handler.getCallCount('setNoBackgroundImage'));
    }

    function assertSetNoBackgroundImageCalled() {
      assertEquals(1, handler.getCallCount('setNoBackgroundImage'));
    }

    test('no background selected by default', () => {
      assertSetNoBackgroundImageNotCalled();
    });

    test('no background selected when clicked', () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('http://a');
      customizeBackgrounds.theme = theme;
      customizeBackgrounds.$.noBackground.click();
      assertSetNoBackgroundImageCalled();
    });

    test('not selected when refresh collection set', () => {
      const theme = createTheme();
      theme.dailyRefreshCollectionId = 'landscape';
      customizeBackgrounds.theme = theme;
      assertSetNoBackgroundImageNotCalled();
    });

    test('not selected when refresh collection set', () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('http://a');
      customizeBackgrounds.theme = theme;
      assertSetNoBackgroundImageNotCalled();
    });
  });
});
