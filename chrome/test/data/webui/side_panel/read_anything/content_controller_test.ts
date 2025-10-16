// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ContentController, ContentType, HIGHLIGHTED_LINK_CLASS, LOG_EMPTY_DELAY_MS, MIN_MS_TO_READ, NodeStore, previousReadHighlightClass, ReadAloudNode, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ContentListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('ContentController', () => {
  let contentController: ContentController;
  let readingMode: FakeReadingMode;
  let nodeStore: NodeStore;
  let speechController: SpeechController;
  let metrics: TestMetricsBrowserProxy;
  let listener: ContentListener;
  let receivedContentStateChange: boolean;
  let receivedNewPageDrawn: boolean;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    metrics = mockMetrics();
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    SpeechBrowserProxyImpl.setInstance(new TestSpeechBrowserProxy());
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    contentController = new ContentController();

    receivedContentStateChange = false;
    receivedNewPageDrawn = false;
    listener = {
      onContentStateChange() {
        receivedContentStateChange = true;
      },
      onNewPageDrawn() {
        receivedNewPageDrawn = true;
      },
    };
    contentController.addListener(listener);
  });

  suite('setEmpty', () => {
    setup(() => {
      contentController.setState(ContentType.HAS_CONTENT);
    });

    test('sets empty state', () => {
      const emptyPath = 'empty_state.svg';

      contentController.setEmpty();

      const empty = contentController.getState();
      assertTrue(contentController.isEmpty());
      assertStringContains(empty.darkImagePath, emptyPath);
      assertStringContains(empty.imagePath, emptyPath);
    });

    test('logs if still empty after delay', () => {
      const mockTimer = new MockTimer();
      mockTimer.install();

      contentController.setEmpty();
      assertTrue(contentController.isEmpty());
      assertEquals(0, metrics.getCallCount('recordEmptyState'));

      mockTimer.tick(LOG_EMPTY_DELAY_MS);
      assertTrue(contentController.isEmpty());
      assertEquals(1, metrics.getCallCount('recordEmptyState'));

      mockTimer.uninstall();
    });

    test('does not log if not empty after delay', () => {
      const mockTimer = new MockTimer();
      mockTimer.install();

      contentController.setEmpty();
      assertEquals(0, metrics.getCallCount('recordEmptyState'));

      contentController.setState(ContentType.HAS_CONTENT);
      mockTimer.tick(LOG_EMPTY_DELAY_MS);
      assertEquals(0, metrics.getCallCount('recordEmptyState'));

      mockTimer.uninstall();
    });

    test('logs empty state once if still empty', () => {
      const mockTimer = new MockTimer();
      mockTimer.install();

      contentController.setEmpty();
      mockTimer.tick(LOG_EMPTY_DELAY_MS);
      assertEquals(1, metrics.getCallCount('recordEmptyState'));
      assertTrue(contentController.isEmpty());

      contentController.setEmpty();
      mockTimer.tick(LOG_EMPTY_DELAY_MS);
      assertEquals(1, metrics.getCallCount('recordEmptyState'));
      assertTrue(contentController.isEmpty());

      mockTimer.uninstall();
    });
  });

  test('setState notifies listeners of state change', () => {
    contentController.setState(ContentType.HAS_CONTENT);
    assertTrue(receivedContentStateChange);

    // Don't notify a second time for the same state.
    receivedContentStateChange = false;
    contentController.setState(ContentType.HAS_CONTENT);
    assertFalse(receivedContentStateChange);
  });

  test('setEmpty depends on google docs', () => {
    chrome.readingMode.isGoogleDocs = true;
    contentController.setEmpty();
    const docsHeading = contentController.getState().heading;

    chrome.readingMode.isGoogleDocs = false;
    contentController.setEmpty();
    const regularHeading = contentController.getState().heading;

    assertNotEquals(docsHeading, regularHeading);
  });

  test('hasContent', () => {
    assertFalse(contentController.hasContent());

    contentController.setState(ContentType.HAS_CONTENT);
    assertTrue(contentController.hasContent());

    contentController.setState(ContentType.LOADING);
    assertFalse(contentController.hasContent());

    contentController.setEmpty();
    assertFalse(contentController.hasContent());
  });

  test('isEmpty', () => {
    assertTrue(contentController.isEmpty());

    contentController.setState(ContentType.HAS_CONTENT);
    assertFalse(contentController.isEmpty());

    contentController.setState(ContentType.LOADING);
    assertFalse(contentController.isEmpty());

    contentController.setEmpty();
    assertTrue(contentController.isEmpty());
  });

  test('onNodeWillBeDeleted removes node', () => {
    const id1 = 10;
    const id2 = 12;
    chrome.readingMode.rootId = id2;
    const node1 = document.createTextNode('Huntrx don\'t miss');
    const node2 = document.createTextNode('How it\'s done done done');
    nodeStore.setDomNode(node1, id1);
    nodeStore.setDomNode(node2, id2);
    contentController.setState(ContentType.HAS_CONTENT);

    contentController.onNodeWillBeDeleted(id1);

    assertFalse(!!nodeStore.getDomNode(id1));
    assertTrue(contentController.hasContent());
  });

  test('onNodeWillBeDeleted shows empty if no more nodes', () => {
    const id = 10;
    const node = document.createTextNode('Huntrx don\'t quit');
    chrome.readingMode.rootId = id;
    nodeStore.setDomNode(node, id);
    contentController.setState(ContentType.HAS_CONTENT);

    contentController.onNodeWillBeDeleted(id);

    assertFalse(!!nodeStore.getDomNode(id));
    assertFalse(contentController.hasContent());
    assertTrue(contentController.isEmpty());
  });

  suite('updateContent', () => {
    const rootId = 29;
    let node: HTMLElement;

    setup(() => {
      node = document.createElement('p');
      const text = document.createTextNode('One swing ahead of the sword');
      node.appendChild(text);
      document.body.appendChild(node);
      chrome.readingMode.rootId = rootId;
    });

    test('logs speech stop if called while speech active', async () => {
      speechController.onPlayPauseToggle(node);

      contentController.updateContent();

      assertEquals(
          chrome.readingMode.unexpectedUpdateContentStopSource,
          await metrics.whenCalled('recordSpeechStopSource'));
    });

    test('does not crash with no root', () => {
      chrome.readingMode.rootId = 0;
      assertFalse(!!contentController.updateContent());
    });

    test('hides loading page', () => {
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'but I bite';
      contentController.setState(ContentType.LOADING);

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertTrue(contentController.hasContent());
      assertFalse(contentController.isEmpty());
    });

    test('sets empty if no content', () => {
      contentController.setState(ContentType.LOADING);

      const root = contentController.updateContent();

      assertFalse(!!root);
      assertFalse(contentController.hasContent());
      assertTrue(contentController.isEmpty());
    });

    test('logs new page with new tree', () => {
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'okay like I know I ramble';

      contentController.updateContent();
      contentController.updateContent();
      assertEquals(1, metrics.getCallCount('recordNewPage'));

      chrome.readingMode.rootId = rootId + 1;
      contentController.updateContent();

      assertEquals(2, metrics.getCallCount('recordNewPage'));
    });

    test('loads images with flag enabled', () => {
      const imgId1 = 89;
      const imgId2 = 88;
      readingMode.imagesFeatureEnabled = true;
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'okay like I know I ramble';

      readingMode.imagesEnabled = true;
      nodeStore.addImageToFetch(imgId1);
      contentController.updateContent();

      readingMode.imagesEnabled = false;
      nodeStore.addImageToFetch(imgId2);
      contentController.updateContent();

      assertArrayEquals([imgId1, imgId2], readingMode.fetchedImages);
    });

    test('notifies listeners of new page drawn', () => {
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'I go Rambo';
      stubAnimationFrame();

      contentController.updateContent();

      assertTrue(receivedNewPageDrawn);
    });

    test('estimates words seen after draw', () => {
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'full of venom';
      stubAnimationFrame();
      const mockTimer = new MockTimer();
      mockTimer.install();
      let sentWordsSeen = false;
      readingMode.updateWordsSeen = () => {
        sentWordsSeen = true;
      };

      contentController.updateContent();
      mockTimer.tick(MIN_MS_TO_READ);
      mockTimer.uninstall();

      assertTrue(sentWordsSeen);
    });

    test('builds a simple text node', () => {
      const text = 'Knockin you out like a lullaby';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals(Node.TEXT_NODE, root.nodeType);
      assertEquals(text, root.textContent);
    });

    test('builds a bolded text node', () => {
      const text = 'Hear that sound ringin in your mind';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;
      readingMode.shouldBold = () => true;

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals('B', root.nodeName);
      assertEquals(text, root.textContent);
    });

    test('builds an overline text node', () => {
      const text = 'Better sit down for the show';
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => text;
      readingMode.isOverline = () => true;

      const root = contentController.updateContent();

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
      chrome.readingMode.rootId = parentId;

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals('P', root.nodeName);
      assertTrue(!!root.firstChild);
      assertEquals(text, root.textContent);
    });

    test('builds a link as an <a> tag when links are shown', () => {
      const childId = 65;
      const url = 'https://www.google.com/';
      chrome.readingMode.linksEnabled = true;
      readingMode.getHtmlTag = (id) => {
        return id === childId ? '' : 'a';
      };
      readingMode.getUrl = () => url;
      readingMode.getTextContent = () => url;
      let clicked = false;
      readingMode.onLinkClicked = () => {
        clicked = true;
      };
      readingMode.getChildren = (id) => {
        return id === childId ? [] : [childId];
      };

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLAnchorElement, 'instance');
      assertEquals(url, root.href);
      root.click();
      assertTrue(clicked, 'clicked');
    });

    test('builds a link as a <span> tag when links are hidden', () => {
      const childId = 71;
      const url = 'https://www.relsilicon.com/';
      chrome.readingMode.linksEnabled = false;
      readingMode.getHtmlTag = (id) => {
        return id === childId ? '' : 'a';
      };
      readingMode.getUrl = () => url;
      readingMode.getTextContent = () => url;
      readingMode.getChildren = (id) => {
        return id === childId ? [] : [childId];
      };

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLSpanElement);
      assertEquals(url, root.dataset['link']);
      assertFalse(!!root.getAttribute('href'));
    });

    test('builds an image as a <canvas> tag', () => {
      const altText = 'how it\'s done done done';
      chrome.readingMode.imagesEnabled = true;
      readingMode.getHtmlTag = () => 'img';
      readingMode.getAltText = () => altText;

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLCanvasElement);
      assertEquals(altText, root.getAttribute('alt'));
      assertEquals('', root.style.display);
      assertTrue(nodeStore.hasImagesToFetch());
      nodeStore.fetchImages();
      assertArrayEquals([rootId], readingMode.fetchedImages);
    });

    test('builds a video as a <canvas> tag', () => {
      const altText = 'Huntrx';
      chrome.readingMode.imagesEnabled = true;
      readingMode.getHtmlTag = () => 'video';
      readingMode.getAltText = () => altText;

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLCanvasElement);
      assertEquals(altText, root.getAttribute('alt'));
      assertEquals('', root.style.display);
      assertTrue(nodeStore.hasImagesToFetch());
      nodeStore.fetchImages();
      assertArrayEquals([rootId], readingMode.fetchedImages);
    });

    test('sets text direction', () => {
      const childId = 70;
      readingMode.getHtmlTag = (id) => {
        return id === childId ? '' : 'p';
      };
      readingMode.getTextDirection = () => 'rtl';
      readingMode.getTextContent = () => 'spittin facts';
      readingMode.getChildren = (id) => {
        return id === childId ? [] : [childId];
      };

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('rtl', root.getAttribute('dir'));
    });

    test('sets the language', () => {
      const childId = 70;
      readingMode.getHtmlTag = (id) => {
        return id === childId ? '' : 'p';
      };
      readingMode.getLanguage = () => 'ko';
      readingMode.getTextContent = () => 'you know that\'s';
      readingMode.getChildren = (id) => {
        return id === childId ? [] : [childId];
      };

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('ko', root.getAttribute('lang'));
    });

    test('builds details as a div', () => {
      const childId = 67;
      readingMode.getHtmlTag = (id) => {
        return id === childId ? '' : 'details';
      };
      readingMode.getChildren = (id) => {
        return id === childId ? [] : [childId];
      };
      readingMode.getTextContent = () => 'I don\'t talk';

      const root = contentController.updateContent();

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
      contentController.setState(ContentType.NO_CONTENT);
      contentController.updateLinks(shadowRoot);
      assertFalse(!!shadowRoot.firstChild);
    });

    test('replaces <a> with <span> when hiding links', () => {
      chrome.readingMode.linksEnabled = false;
      shadowRoot.appendChild(link);
      nodeStore.setDomNode(link, linkId);

      contentController.setState(ContentType.HAS_CONTENT);
      contentController.updateLinks(shadowRoot);

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

      contentController.setState(ContentType.HAS_CONTENT);
      contentController.updateLinks(shadowRoot);

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

      contentController.setState(ContentType.HAS_CONTENT);
      contentController.updateLinks(shadowRoot);

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

      contentController.setState(ContentType.HAS_CONTENT);
      contentController.updateLinks(shadowRoot);

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

          contentController.setState(ContentType.HAS_CONTENT);
          contentController.updateLinks(shadowRoot);

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

          contentController.setState(ContentType.HAS_CONTENT);
          contentController.updateLinks(shadowRoot);

          const newInnerSpan = shadowRoot.querySelector('a span');
          assertTrue(!!newInnerSpan);
          assertFalse(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
          assertFalse(
              newInnerSpan.classList.contains(previousReadHighlightClass));
        });

    test(
        'does not add previous highlighting when hiding links that were ' +
            'highlighted and then cleared',
        () => {
          const innerSpan = document.createElement('span');
          innerSpan.classList.add(HIGHLIGHTED_LINK_CLASS);
          link.appendChild(innerSpan);
          shadowRoot.appendChild(link);
          nodeStore.setDomNode(link, linkId);
          chrome.readingMode.linksEnabled = false;

          contentController.onSelectionChange(shadowRoot);
          contentController.setState(ContentType.HAS_CONTENT);
          contentController.updateLinks(shadowRoot);

          const newInnerSpan = shadowRoot.querySelector('span[data-link] span');
          assertTrue(!!newInnerSpan);
          assertFalse(
              newInnerSpan.classList.contains(previousReadHighlightClass));
          assertFalse(newInnerSpan.classList.contains(HIGHLIGHTED_LINK_CLASS));
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
      contentController.setState(ContentType.HAS_CONTENT);

      contentController.updateImages(shadowRoot);
      await microtasksFinished();

      assertEquals('none', canvas.style.display);
      assertEquals('none', figure.style.display);
      assertTrue(nodeStore.areNodesAllHidden(
          [ReadAloudNode.createFromAxNode(textId)!]));
    });

    test('shows images and clears hidden nodes when enabled', async () => {
      chrome.readingMode.imagesFeatureEnabled = true;
      chrome.readingMode.imagesEnabled = true;
      nodeStore.hideImageNode(textId);
      canvas.style.display = 'none';
      figure.style.display = 'none';
      contentController.setState(ContentType.HAS_CONTENT);

      contentController.updateImages(shadowRoot);
      await microtasksFinished();

      assertEquals('', canvas.style.display);
      assertEquals('', figure.style.display);
      assertFalse(nodeStore.areNodesAllHidden(
          [ReadAloudNode.createFromAxNode(textId)!]));
    });
  });
});
