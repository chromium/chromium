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
  let receivedContentChange: boolean;

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
    receivedContentChange = false;
    listener = {
      onContentStateChange() {
        receivedContentStateChange = true;
      },
      onNewPageDrawn() {
        receivedNewPageDrawn = true;
      },
      onContentChange() {
        receivedContentChange = true;
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

  test('onNodeWillBeDeleted notifies of new content', () => {
    const id = 12;
    chrome.readingMode.rootId = id;
    const node = document.createTextNode('How it\'s done done done');
    nodeStore.setDomNode(node, id);
    contentController.setState(ContentType.HAS_CONTENT);

    contentController.onNodeWillBeDeleted(id);

    assertTrue(receivedContentChange);
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

    test('notifies listeners of new content', () => {
      readingMode.getHtmlTag = () => '';
      readingMode.getTextContent = () => 'I go Rambo';
      stubAnimationFrame();

      contentController.updateContent();

      assertTrue(receivedContentChange);
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

    test('builds an input as a <div> tag', () => {
      const rootId = 5;
      const childId = 7;
      const inputText = 'For her';

      // Set up the AX Tree with an input that has a text child.
      readingMode.rootId = rootId;
      readingMode.getHtmlTag = (id: number) => {
        if (id === rootId) {
          return 'input';
        }
        if (id === childId) {
          return '';  // Text node
        }
        return '';
      };
      readingMode.getTextContent = (id: number) => {
        if (id === childId) {
          return inputText;
        }
        return '';
      };
      readingMode.getChildren = (id: number) => {
        if (id === rootId) {
          return [childId];
        }
        return [];
      };
      const root = contentController.updateContent();
      assertTrue(root instanceof HTMLDivElement);
      assertEquals(inputText, root.textContent);
    });

    test('link visibility toggled toggles links with Readability', async () => {
      const url = 'https://www.relsilicon.com/';
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
      contentController.configureTrustedTypes();
      const text = 'a link';
      readingMode.htmlContent = `<a href="${url}">${text}</a>`;

      const root = contentController.updateContent();
      await microtasksFinished();
      assertTrue(!!root);
      const container = document.createElement('div');
      document.body.appendChild(container);
      const shadowRoot = container.attachShadow({mode: 'open'});
      const contentDiv = (root as DocumentFragment).querySelector('div');
      assertTrue(!!contentDiv);
      shadowRoot.append(...contentDiv.childNodes);

      // Hide the links.
      chrome.readingMode.linksEnabled = false;
      contentController.updateLinks(shadowRoot);
      let link = shadowRoot.querySelector('a');
      assertFalse(!!link);
      let span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
      assertTrue(!!span);
      assertEquals(url, span.dataset['link']);
      assertEquals(text, span.textContent);

      // Show the links.
      chrome.readingMode.linksEnabled = true;
      contentController.updateLinks(shadowRoot);
      span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
      assertFalse(!!span);
      link = shadowRoot.querySelector('a');
      assertTrue(!!link);
      assertEquals(url, link.href);
      assertEquals(text, link.textContent);
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

    test('builds a button as a <div> tag', () => {
      const rootId = 5;
      const childId = 7;
      const buttonText = 'Automatic';

      // Set up the AX Tree with a button that has a text child.
      readingMode.rootId = rootId;
      readingMode.getHtmlTag = (id: number) => {
        if (id === rootId) {
          return 'button';
        }
        if (id === childId) {
          return '';  // Text node
        }
        return '';
      };
      readingMode.getTextContent = (id: number) => {
        if (id === childId) {
          return buttonText;
        }
        return '';
      };
      readingMode.getChildren = (id: number) => {
        if (id === rootId) {
          return [childId];
        }
        return [];
      };
      const root = contentController.updateContent();
      assertTrue(root instanceof HTMLDivElement);
      assertEquals(buttonText, root.textContent);
    });

    test(
        'builds a button as a <div> tag when Readability enabled', async () => {
          chrome.readingMode.isReadabilityEnabled = true;
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeReadability;
          const buttonText = 'Buttons should be seen and not clicked';
          contentController.configureTrustedTypes();
          readingMode.htmlContent = `<button>${buttonText}</button>`;

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          assertFalse(!!(root as DocumentFragment).querySelector('button'));
          const newDiv = (root as DocumentFragment).querySelector('div > div');
          assertTrue(!!newDiv);
          assertEquals(buttonText, newDiv.textContent);
        });

    test(
        'builds a mark tag as a <div> tag when Readability enabled',
        async () => {
          chrome.readingMode.isReadabilityEnabled = true;
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeReadability;
          const markText = 'When everything is important, nothing is';
          contentController.configureTrustedTypes();
          readingMode.htmlContent = `<mark>${markText}</mark>`;

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          assertFalse(!!(root as DocumentFragment).querySelector('mark'));
          const newSpan = (root as DocumentFragment).querySelector('div > div');
          assertTrue(!!newSpan);
          assertEquals(markText, newSpan.textContent);
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

    test(
        'honors links disabled preference on first open with Readability',
        async () => {
          const url = 'https://www.google.com/';
          const text = 'best link ever';
          chrome.readingMode.isReadabilityEnabled = true;
          chrome.readingMode.isReadabilityWithLinksEnabled = false;
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeReadability;
          contentController.configureTrustedTypes();
          readingMode.htmlContent = `<a href="${url}">${text}</a>`;
          chrome.readingMode.linksEnabled = false;

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          const container = document.createElement('div');
          document.body.appendChild(container);
          const shadowRoot = container.attachShadow({mode: 'open'});
          const contentDiv = (root as DocumentFragment).querySelector('div');
          assertTrue(!!contentDiv);
          shadowRoot.append(...contentDiv.childNodes);

          const link = shadowRoot.querySelector('a');
          assertFalse(!!link);
          const span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
          assertTrue(!!span);
          assertEquals(url, span.dataset['link']);
          assertEquals(text, span.textContent);
        });

    test(
        'honors links enabled preference on first open with Readability with links enabled',
        async () => {
          const url = 'https://www.google.com/';
          const text = 'best link ever';
          chrome.readingMode.isReadabilityEnabled = true;
          chrome.readingMode.isReadabilityWithLinksEnabled = true;
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeReadability;
          contentController.configureTrustedTypes();
          readingMode.htmlContent = `<a href="${url}">${text}</a>`;
          chrome.readingMode.linksEnabled = true;

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          const container = document.createElement('div');
          document.body.appendChild(container);
          const shadowRoot = container.attachShadow({mode: 'open'});
          const contentDiv = (root as DocumentFragment).querySelector('div');
          assertTrue(!!contentDiv);
          shadowRoot.append(...contentDiv.childNodes);

          const link = shadowRoot.querySelector('a');
          assertTrue(!!link);
          assertEquals(url, link.href);
          assertEquals(text, link.textContent);
        });

    test('links are disabled when ReadabilityWithLinks is false', async () => {
      const url = 'https://www.google.com/';
      const text = 'best link ever';
      chrome.readingMode.isReadabilityEnabled = true;
      chrome.readingMode.isReadabilityWithLinksEnabled = false;
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
      contentController.configureTrustedTypes();
      readingMode.htmlContent = `<a href="${url}">${text}</a>`;
      chrome.readingMode.linksEnabled = true;

      const root = contentController.updateContent();
      await microtasksFinished();

      assertTrue(!!root);
      const container = document.createElement('div');
      document.body.appendChild(container);
      const shadowRoot = container.attachShadow({mode: 'open'});
      const contentDiv = (root as DocumentFragment).querySelector('div');
      assertTrue(!!contentDiv);
      shadowRoot.append(...contentDiv.childNodes);

      // There should be no `<a>` tag.
      const link = shadowRoot.querySelector('a');
      assertFalse(!!link);
      // Instead there should be a `<span>` tag.
      const span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
      assertTrue(!!span);
      assertEquals(url, span.dataset['link']);
      assertEquals(text, span.textContent);
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
      assertTrue(receivedContentChange);
    });

    test('does nothing if element is missing', async () => {
      await contentController.onImageDownloaded(nodeId);
      await microtasksFinished();

      assertFalse(drewImage);
      assertFalse(receivedContentChange);
    });

    test('does nothing if element is not a canvas', async () => {
      const element = document.createElement('p');
      nodeStore.setDomNode(element, nodeId);

      await contentController.onImageDownloaded(nodeId);
      await microtasksFinished();

      assertFalse(drewImage);
      assertFalse(receivedContentChange);
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
      assertTrue(receivedContentChange);
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
      assertTrue(receivedContentChange);
    });

    test('notifies of content change with readability', async () => {
      chrome.readingMode.imagesFeatureEnabled = true;
      chrome.readingMode.imagesEnabled = false;
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
      contentController.setState(ContentType.HAS_CONTENT);
      receivedContentChange = false;

      contentController.updateImages(shadowRoot);
      await microtasksFinished();

      assertTrue(receivedContentChange);
    });
  });

  suite('updateAnchorsForReadability', () => {
    let container: HTMLElement;
    let anchor: HTMLAnchorElement;
    const url = 'https://www.google.com/';
    const axId = 100;

    setup(() => {
      container = document.createElement('div');
      anchor = document.createElement('a');
      anchor.href = url;
      container.appendChild(anchor);

      chrome.readingMode.isReadabilityEnabled = true;
      chrome.readingMode.isReadabilityWithLinksEnabled = true;
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
      chrome.readingMode.axTreeAnchors = {};
      contentController.setState(ContentType.HAS_CONTENT);
    });

    test('associates dom node when 1:1 match exists', () => {
      chrome.readingMode.axTreeAnchors = {[url]: [{axId: axId, name: 'text'}]};
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor, nodeStore.getDomNode(axId));
    });

    test('resolves ambiguity using HTML ID match (highest priority)', () => {
      anchor.id = 'correct-id';
      chrome.readingMode.axTreeAnchors = {
        [url]: [
          {axId: 200, htmlId: 'wrong-id', name: 'same text'},
          {axId: axId, htmlId: 'correct-id', name: 'same text'},
        ],
      };
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor, nodeStore.getDomNode(axId));
      assertFalse(!!nodeStore.getDomNode(200));
    });

    test('resolves ambiguity using text content match', () => {
      anchor.textContent = 'Click Here';
      chrome.readingMode.axTreeAnchors = {
        [url]:
            [{axId: 200, name: 'Read More'}, {axId: axId, name: 'Click Here'}],
      };
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor, nodeStore.getDomNode(axId));
    });

    test('resolves ambiguity using surrounding text context', () => {
      const textNode = document.createTextNode('Previous Text');
      container.insertBefore(textNode, anchor);
      anchor.textContent = 'Link';

      chrome.readingMode.axTreeAnchors = {
        [url]: [
          {axId: 200, name: 'Link', textBefore: 'Wrong Context'},
          {axId: axId, name: 'Link', textBefore: 'Previous Text'},
        ],
      };
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor, nodeStore.getDomNode(axId));
    });

    test('does not associate node when strictly ambiguous (tie score)', () => {
      anchor.textContent = 'Ambiguous';
      chrome.readingMode.axTreeAnchors = {
        [url]:
            [{axId: axId, name: 'Ambiguous'}, {axId: 101, name: 'Ambiguous'}],
      };
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
      assertFalse(!!nodeStore.getDomNode(101));
    });

    test('matches multiple anchors correctly by consuming candidates', () => {
      container.replaceChildren();
      const anchor1 = document.createElement('a');
      anchor1.href = url;
      anchor1.textContent = 'First Link';
      container.appendChild(anchor1);

      const anchor2 = document.createElement('a');
      anchor2.href = url;
      anchor2.textContent = 'Second Link';
      container.appendChild(anchor2);

      chrome.readingMode.axTreeAnchors = {
        [url]:
            [{axId: 100, name: 'First Link'}, {axId: 200, name: 'Second Link'}],
      };
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor1, nodeStore.getDomNode(100));
      assertEquals(anchor2, nodeStore.getDomNode(200));
    });

    test('does not associate node when no match exists in map', () => {
      chrome.readingMode
          .axTreeAnchors = {['https://other.com/']: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test('converts anchor to span when no matching URL is found', () => {
      chrome.readingMode.axTreeAnchors = {};
      anchor.textContent = 'Text with no URL';
      contentController.updateAnchorsForReadability(container);
      const spans = container.querySelectorAll('span');
      const anchors = container.querySelectorAll('a');

      assertFalse(!!nodeStore.getDomNode(axId));
      assertTrue(!!spans[0]);
      assertEquals(1, spans.length);
      assertEquals(0, anchors.length);
      assertEquals('Text with no URL', spans[0].textContent);
    });

    test('converts anchor to span when href is empty', () => {
      anchor.removeAttribute('href');
      contentController.updateAnchorsForReadability(container);
      const anchors = container.querySelectorAll('a');
      const spans = container.querySelectorAll('span');

      assertFalse(!!nodeStore.getDomNode(axId));
      assertEquals(0, anchors.length);
      assertEquals(1, spans.length);
    });

    test(
        'converts anchor to span when URL is not present in axTreeAnchors',
        () => {
          chrome.readingMode.axTreeAnchors = {
            'https://www.wasteheadquarters.com/': [{axId: 999, name: 'waste'}],
          };
          contentController.updateAnchorsForReadability(container);
          const anchors = container.querySelectorAll('a');
          const spans = container.querySelectorAll('span');

          assertFalse(!!nodeStore.getDomNode(axId));
          assertEquals(0, anchors.length);
          assertEquals(1, spans.length);
        });

    test('does nothing if not in Readability mode', () => {
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeScreen2x;
      chrome.readingMode.axTreeAnchors = {[url]: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test('does nothing if Readability is not enabled', () => {
      chrome.readingMode.isReadabilityEnabled = false;
      chrome.readingMode.axTreeAnchors = {[url]: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test(
        'does nothing if Readability is enabled but links are disabled', () => {
          chrome.readingMode.isReadabilityWithLinksEnabled = false;
          chrome.readingMode.axTreeAnchors = {[url]: [{axId: axId}]};
          contentController.updateAnchorsForReadability(container);

          assertFalse(!!nodeStore.getDomNode(axId));
        });
  });
});
