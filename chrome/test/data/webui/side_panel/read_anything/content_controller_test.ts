// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ContentController, HIGHLIGHTED_LINK_CLASS, NodeStore, previousReadHighlightClass, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('ContentController', () => {
  let contentController: ContentController;
  let readingMode: FakeReadingMode;
  let nodeStore: NodeStore;
  let speechController: SpeechController;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    SpeechBrowserProxyImpl.setInstance(new TestSpeechBrowserProxy());
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    contentController = new ContentController();
  });

  suite('buildSubtree', () => {
    const nodeId = 29;

    test('builds a simple text node', () => {
      const text = 'Knockin you out like a lullaby';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;

      const root = contentController.buildSubtree(nodeId);

      assertEquals(Node.TEXT_NODE, root.nodeType);
      assertEquals(text, root.textContent);
    });

    test('builds a bolded text node', () => {
      const text = 'Hear that sound ringin in your mind';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;
      readingMode.shouldBold = () => true;

      const root = contentController.buildSubtree(nodeId);

      assertEquals('B', root.nodeName);
      assertEquals(text, root.textContent);
    });

    test('builds an overline text node', () => {
      const text = 'Better sit down for the show';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;
      readingMode.isOverline = () => true;

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLSpanElement);
      assertEquals(text, root.textContent);
      assertEquals('overline', root.style.textDecoration);
    });

    test('builds an element with a text child', () => {
      const parentId = 10;
      const childId = 11;
      const text = 'Run, run, we run the town';
      // Parent is a <p> tag with one text child.
      readingMode.getHtmlTag = (id) => {
        return id === parentId ? 'p' : '';
      };
      readingMode.getChildren = (id) => {
        return id === parentId ? [childId] : [];
      };
      readingMode.getTextContent = (id) => {
        return id === childId ? text : '';
      };

      const root = contentController.buildSubtree(parentId);

      assertEquals('P', root.nodeName);
      assertTrue(!!root.firstChild);
      assertEquals(text, root.textContent);
    });

    test('builds a link as an <a> tag when links are shown', () => {
      const url = 'https://www.google.com/';
      chrome.readingMode.linksEnabled = true;
      readingMode.getHtmlTag = () => 'a';
      readingMode.getUrl = () => url;
      let clicked = false;
      readingMode.onLinkClicked = () => {
        clicked = true;
      };

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLAnchorElement);
      assertEquals(url, root.href);
      assertTrue(!!root.onclick);
      root.onclick(new MouseEvent('type'));
      assertTrue(clicked);
    });

    test('builds a link as a <span> tag when links are hidden', () => {
      const url = 'https://www.relsilicon.com/';
      chrome.readingMode.linksEnabled = false;
      readingMode.getHtmlTag = () => 'a';
      readingMode.getUrl = () => url;

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLSpanElement);
      assertEquals(url, root.dataset['link']);
      assertFalse(!!root.getAttribute('href'));
    });

    test('builds a link as a <span> tag when speech is playing', () => {
      const url = 'https://www.usecheeky.com/';
      chrome.readingMode.linksEnabled = true;
      speechController.onPlayPauseToggle(null, 'Cause I\'m gonna show you');
      readingMode.getHtmlTag = () => 'a';
      readingMode.getUrl = () => url;

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLSpanElement);
      assertEquals(url, root.dataset['link']);
      assertFalse(!!root.getAttribute('href'));
    });

    test('builds an image as a <canvas> tag', () => {
      const altText = 'how it\'s done done done';
      chrome.readingMode.imagesEnabled = true;
      readingMode.getHtmlTag = () => 'img';
      readingMode.getAltText = () => altText;

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLCanvasElement);
      assertEquals(altText, root.getAttribute('alt'));
      assertEquals('', root.style.display);
      assertTrue(nodeStore.hasImagesToFetch());
      nodeStore.fetchImages();
      assertArrayEquals([nodeId], readingMode.fetchedImages);
    });

    test('sets text direction', () => {
      readingMode.getHtmlTag = () => 'p';
      readingMode.getTextDirection = () => 'rtl';

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('rtl', root.getAttribute('dir'));
    });

    test('sets the language', () => {
      readingMode.getHtmlTag = () => 'p';
      readingMode.getLanguage = () => 'ko';

      const root = contentController.buildSubtree(nodeId);

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('ko', root.getAttribute('lang'));
    });

    test('builds details as a div', () => {
      readingMode.getHtmlTag = () => 'details';
      const root = contentController.buildSubtree(nodeId);
      assertTrue(root instanceof HTMLDivElement);
    });
  });

  suite('updateLinks', () => {
    const linkId = 52;
    const linkUrl = 'https://www.docs.google.com/';
    let link: HTMLAnchorElement;
    let shadowRoot: ShadowRoot;

    setup(() => {
      const container = document.createElement('div');
      document.body.appendChild(container);
      shadowRoot = container.attachShadow({mode: 'open'});
      link = document.createElement('a');
      link.href = linkUrl;
      readingMode.getHtmlTag = () => 'a';
      readingMode.getUrl = () => linkUrl;
    });

    test('does nothing if no content', () => {
      chrome.readingMode.linksEnabled = false;
      contentController.updateLinks(false, shadowRoot);
      assertFalse(!!shadowRoot.firstChild);
    });

    test('replaces <a> with <span> when hiding links', () => {
      chrome.readingMode.linksEnabled = false;
      shadowRoot.appendChild(link);
      nodeStore.setDomNode(link, linkId);

      contentController.updateLinks(true, shadowRoot);

      assertFalse(!!shadowRoot.querySelector('a'));
      const span = shadowRoot.querySelector('span[data-link]');
      assertTrue(span instanceof HTMLSpanElement);
      assertEquals(linkUrl, span.dataset['link']);
    });

    test('replaces <span> with <a> when showing links', () => {
      const span = document.createElement('span');
      span.dataset['link'] = linkUrl;
      shadowRoot.appendChild(span);
      nodeStore.setDomNode(span, linkId);
      chrome.readingMode.linksEnabled = true;

      contentController.updateLinks(true, shadowRoot);

      assertFalse(!!shadowRoot.querySelector('span[data-link]'));
      const link = shadowRoot.querySelector('a');
      assertTrue(!!link);
      assertEquals(linkUrl, link.href);
    });

    test('restores previous highlighting when hiding links', () => {
      const innerSpan = document.createElement('span');
      innerSpan.classList.add(HIGHLIGHTED_LINK_CLASS);
      link.appendChild(innerSpan);
      shadowRoot.appendChild(link);
      nodeStore.setDomNode(link, linkId);
      chrome.readingMode.linksEnabled = false;

      contentController.updateLinks(true, shadowRoot);

      const newInnerSpan = shadowRoot.querySelector('span[data-link] span');
      assertTrue(!!newInnerSpan);
      assertTrue(newInnerSpan.classList.contains(previousReadHighlightClass));
      assertFalse(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
    });

    test('removes previous highlighting when showing links', () => {
      const innerSpan = document.createElement('span');
      innerSpan.classList.add(previousReadHighlightClass);
      const outerSpan = document.createElement('span');
      outerSpan.dataset['link'] = linkUrl;
      outerSpan.appendChild(innerSpan);
      shadowRoot.appendChild(outerSpan);
      nodeStore.setDomNode(outerSpan, linkId);
      chrome.readingMode.linksEnabled = true;

      contentController.updateLinks(true, shadowRoot);

      const newInnerSpan = shadowRoot.querySelector('a span');
      assertTrue(!!newInnerSpan);
      assertTrue(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
      assertFalse(newInnerSpan.classList.contains(previousReadHighlightClass));
    });

    test(
        'does not add previous highlighting when hiding links that were not' +
            ' highlighted',
        () => {
          const innerSpan = document.createElement('span');
          link.appendChild(innerSpan);
          shadowRoot.appendChild(link);
          nodeStore.setDomNode(link, linkId);
          chrome.readingMode.linksEnabled = false;

          contentController.updateLinks(true, shadowRoot);

          const newInnerSpan = shadowRoot.querySelector('span[data-link] span');
          assertTrue(!!newInnerSpan);
          assertFalse(
              newInnerSpan.classList.contains(previousReadHighlightClass));
          assertFalse(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
        });

    test(
        'does not mark as highlighted when showing links that were not' +
            ' highlighted',
        () => {
          const innerSpan = document.createElement('span');
          const outerSpan = document.createElement('span');
          outerSpan.dataset['link'] = linkUrl;
          outerSpan.appendChild(innerSpan);
          shadowRoot.appendChild(outerSpan);
          nodeStore.setDomNode(outerSpan, linkId);
          chrome.readingMode.linksEnabled = true;

          contentController.updateLinks(true, shadowRoot);

          const newInnerSpan = shadowRoot.querySelector('a span');
          assertTrue(!!newInnerSpan);
          assertFalse(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
          assertFalse(
              newInnerSpan.classList.contains(previousReadHighlightClass));
        });
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
