// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ContentController, NodeStore} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('ContentController', () => {
  let contentController: ContentController;
  let readingMode: FakeReadingMode;
  let nodeStore: NodeStore;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    contentController = new ContentController();
  });

  suite('loadImages', () => {
    test('does nothing if images feature is disabled', () => {
      chrome.readingMode.imagesFeatureEnabled = false;
      nodeStore.addImageToFetch(12);

      contentController.loadImages();

      assertEquals(0, readingMode.fetchedImages.length);
    });

    test('fetches images if feature is enabled', () => {
      chrome.readingMode.imagesFeatureEnabled = true;
      const imageId = 33;
      nodeStore.addImageToFetch(imageId);

      contentController.loadImages();

      assertArrayEquals([imageId], readingMode.fetchedImages);
    });
  });

  suite('onImageDownloaded', () => {
    const nodeId = 5;
    const imageData = {
      data: new Uint8ClampedArray([255, 0, 0, 255]),
      width: 1,
      height: 1,
      scale: 1.5,
    };
    let canvas: HTMLCanvasElement;
    let drewImage: boolean;

    setup(() => {
      canvas = document.createElement('canvas');
      chrome.readingMode.getImageBitmap = () => imageData;
      const context = canvas.getContext('2d');
      assertTrue(!!context);
      drewImage = false;
      context.drawImage = () => {
        drewImage = true;
      };
    });

    test('updates canvas when data and element are valid', async () => {
      nodeStore.setDomNode(canvas, nodeId);

      await contentController.onImageDownloaded(nodeId);
      await microtasksFinished();

      assertEquals(imageData.width, canvas.width);
      assertEquals(imageData.height, canvas.height);
      assertEquals(imageData.scale.toString(), canvas.style.zoom);
      assertTrue(drewImage);
    });

    test('does nothing if element is missing', async () => {
      await contentController.onImageDownloaded(nodeId);
      await microtasksFinished();
      assertFalse(drewImage);
    });

    test('does nothing if element is not a canvas', async () => {
      const element = document.createElement('p');
      nodeStore.setDomNode(element, nodeId);

      await contentController.onImageDownloaded(nodeId);
      await microtasksFinished();

      assertFalse(drewImage);
    });
  });

  suite('updateImages', () => {
    const id1 = 2;
    const textId = 4;
    const id3 = 6;
    let shadowRoot: ShadowRoot;
    let canvas: HTMLCanvasElement;
    let figure: HTMLElement;
    let textNode: Text;

    setup(() => {
      const container = document.createElement('div');
      document.body.appendChild(container);
      shadowRoot = container.attachShadow({mode: 'open'});

      canvas = document.createElement('canvas');
      textNode = document.createTextNode('Canvas text');
      canvas.appendChild(textNode);
      shadowRoot.appendChild(canvas);

      figure = document.createElement('figure');
      shadowRoot.appendChild(figure);

      // Associate nodes with IDs for the test.
      nodeStore.setDomNode(canvas, id1);
      nodeStore.setDomNode(textNode, textId);
      nodeStore.setDomNode(figure, id3);
    });

    test('hides images and associated text nodes when disabled', async () => {
      chrome.readingMode.imagesFeatureEnabled = true;
      chrome.readingMode.imagesEnabled = false;

      contentController.updateImages(true, shadowRoot);
      await microtasksFinished();

      assertEquals('none', canvas.style.display);
      assertEquals('none', figure.style.display);
      assertTrue(nodeStore.areNodesAllHidden([textId]));
    });

    test('shows images and clears hidden nodes when enabled', async () => {
      chrome.readingMode.imagesFeatureEnabled = true;
      chrome.readingMode.imagesEnabled = true;
      nodeStore.hideImageNode(textId);
      canvas.style.display = 'none';
      figure.style.display = 'none';

      contentController.updateImages(true, shadowRoot);
      await microtasksFinished();

      assertEquals('', canvas.style.display);
      assertEquals('', figure.style.display);
      assertFalse(nodeStore.areNodesAllHidden([textId]));
    });
  });
});
