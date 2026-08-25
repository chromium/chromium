// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {AudioBrowserProxyImpl, ContentBrowserProxyImpl, ContentController, ContentType, HIGHLIGHTED_LINK_CLASS, LOG_EMPTY_DELAY_MS, MIN_MS_TO_READ, NodeStore, previousReadHighlightClass, ReadAloudNode, SpeechBrowserProxyImpl, SpeechController, VisualBrowserProxyImpl} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ContentListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics, stubAnimationFrame} from './common.js';
import {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('ContentController', () => {
  let contentController: ContentController;
  let nodeStore: NodeStore;
  let speechController: SpeechController;
  let metrics: TestMetricsBrowserProxy;
  let listener: ContentListener;
  let receivedContentStateChange: boolean;
  let receivedNewPageDrawn: boolean;
  let receivedContentChange: boolean;
  let contentBrowserProxy: TestContentBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    contentBrowserProxy = new TestContentBrowserProxy();
    ContentBrowserProxyImpl.setInstance(contentBrowserProxy);
    visualBrowserProxy = new TestVisualBrowserProxy();
    VisualBrowserProxyImpl.setInstance(visualBrowserProxy);
    AudioBrowserProxyImpl.setInstance(new TestAudioBrowserProxy());

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
    contentBrowserProxy.googleDocs = true;
    contentController.setEmpty();
    const docsHeading = contentController.getState().heading;

    contentBrowserProxy.googleDocs = false;
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
    contentBrowserProxy.rootId = id2;
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
    contentBrowserProxy.rootId = id;
    nodeStore.setDomNode(node, id);
    contentController.setState(ContentType.HAS_CONTENT);

    contentController.onNodeWillBeDeleted(id);

    assertFalse(!!nodeStore.getDomNode(id));
    assertFalse(contentController.hasContent());
    assertTrue(contentController.isEmpty());
  });

  test('onNodeWillBeDeleted shows empty if only whitespace nodes', () => {
    const id = 10;
    const node = document.createTextNode('   ');
    contentBrowserProxy.rootId = id;
    nodeStore.setDomNode(node, id);
    contentController.setState(ContentType.HAS_CONTENT);

    contentController.onNodeWillBeDeleted(id);

    assertFalse(!!nodeStore.getDomNode(id));
    assertFalse(contentController.hasContent());
    assertTrue(contentController.isEmpty());
  });

  test('onNodeWillBeDeleted notifies of new content', () => {
    const id = 12;
    contentBrowserProxy.rootId = id;
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
      contentBrowserProxy.rootId = rootId;
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeScreen2x;
      speechController.clearReadAloudState();
    });

    test('logs speech stop if called while speech active', async () => {
      speechController.onPlayPauseToggle(node);

      contentController.updateContent();

      assertEquals(
          contentBrowserProxy.unexpectedUpdateContentStopSource,
          await metrics.whenCalled('recordSpeechStopSource'));
    });

    test('does not crash with no root', () => {
      contentBrowserProxy.rootId = 0;
      assertFalse(!!contentController.updateContent());
    });

    test('hides loading page', () => {
      contentBrowserProxy
          .textContentMap = {[contentBrowserProxy.rootId]: 'but I bite'};
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

    test('sets empty if only whitespace content', () => {
      contentBrowserProxy
          .textContentMap = {[contentBrowserProxy.rootId]: '   '};
      contentController.setState(ContentType.LOADING);

      const root = contentController.updateContent();

      assertFalse(!!root);
      assertFalse(contentController.hasContent());
      assertTrue(contentController.isEmpty());
    });

    test('sets empty if only whitespace content with readability', () => {
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.htmlContent = '   ';
      contentController.setState(ContentType.LOADING);

      const root = contentController.updateContent();

      assertFalse(!!root);
      assertFalse(contentController.hasContent());
      assertTrue(contentController.isEmpty());
    });

    test('sets empty if whitespace content with tags in readability', () => {
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.htmlContent = '<div>   </div>';
      contentController.setState(ContentType.LOADING);

      const root = contentController.updateContent();

      assertFalse(!!root);
      assertFalse(contentController.hasContent());
      assertTrue(contentController.isEmpty());
    });

    test(
        'Readability replaces single newlines but keeps consecutive newlines',
        async () => {
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent =
              'I see my present\npartner\n\nin the imperfect tense';

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          const contentDiv = (root as DocumentFragment).querySelector('div');
          assertTrue(!!contentDiv);
          assertEquals(
              'I see my present partner\n\nin the imperfect tense',
              contentDiv.textContent);
        });

    test(
        'Readability does not replace single newlines inside pre tags',
        async () => {
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent =
              '<pre>I see my present\npartner\n\nin the imperfect tense</pre>';

          const root = contentController.updateContent();
          await microtasksFinished();

          assertTrue(!!root);
          const contentDiv = (root as DocumentFragment).querySelector('div');
          assertTrue(!!contentDiv);
          assertEquals(
              'I see my present\npartner\n\nin the imperfect tense',
              contentDiv.textContent);
        });

    test('logs new page with new tree', () => {
      contentBrowserProxy.textContentMap = {
        [rootId]: 'okay like I know I ramble',
        [rootId + 1]: 'okay like I know I ramble',
      };

      contentController.updateContent();
      contentController.updateContent();
      assertEquals(1, metrics.getCallCount('recordNewPage'));

      contentBrowserProxy.rootId = rootId + 1;
      contentController.updateContent();

      assertEquals(2, metrics.getCallCount('recordNewPage'));
    });

    test('loads images with flag enabled', () => {
      const rootId = 1;
      const imgId1 = 89;
      const imgId2 = 88;
      const textId = 90;
      contentBrowserProxy.rootId = rootId;

      contentBrowserProxy.htmlTagMap = {
        [rootId]: 'div',
        [imgId1]: 'img',
        [imgId2]: 'img',
      };
      contentBrowserProxy.textContentMap = {
        [textId]: 'I don\'t own a motorbike.',
      };

      // So that nodeStore.addImageToFetch(imgId1) is called in updateContent
      contentBrowserProxy.childrenMap = {
        [rootId]: [imgId1, textId],
      };

      visualBrowserProxy.imagesEnabled = true;
      contentController.updateContent();

      // So that nodeStore.addImageToFetch(imgId2) is called in updateContent
      contentBrowserProxy.childrenMap = {
        [rootId]: [imgId2, textId],
      };
      visualBrowserProxy.imagesEnabled = false;
      contentController.updateContent();

      assertArrayEquals([imgId1, imgId2], visualBrowserProxy.fetchedImages);
    });

    test('notifies listeners of new page drawn', () => {
      contentBrowserProxy
          .textContentMap = {[contentBrowserProxy.rootId]: 'I go Rambo'};
      stubAnimationFrame();

      contentController.updateContent();

      assertTrue(receivedNewPageDrawn);
    });

    test('notifies listeners of new content', () => {
      contentBrowserProxy
          .textContentMap = {[contentBrowserProxy.rootId]: 'I go Rambo'};
      stubAnimationFrame();

      contentController.updateContent();

      assertTrue(receivedContentChange);
    });

    test('estimates words seen after draw', () => {
      contentBrowserProxy
          .textContentMap = {[contentBrowserProxy.rootId]: 'full of venom'};
      stubAnimationFrame();
      const mockTimer = new MockTimer();
      mockTimer.install();

      contentController.updateContent();
      mockTimer.tick(MIN_MS_TO_READ);
      mockTimer.uninstall();

      assertEquals(1, metrics.getCallCount('updateWordsSeen'));
    });

    test('builds a simple text node', () => {
      const text = 'Knockin you out like a lullaby';
      contentBrowserProxy.textContentMap = {[contentBrowserProxy.rootId]: text};

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals(Node.TEXT_NODE, root.nodeType);
      assertEquals(text, root.textContent);
    });

    test('builds a bolded text node', () => {
      const text = 'Hear that sound ringin in your mind';
      contentBrowserProxy.textContentMap = {[contentBrowserProxy.rootId]: text};
      contentBrowserProxy.shouldBoldVal = true;

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals('B', root.nodeName);
      assertEquals(text, root.textContent);
    });

    test('builds an overline text node', () => {
      const text = 'Better sit down for the show';
      contentBrowserProxy.textContentMap = {[contentBrowserProxy.rootId]: text};
      contentBrowserProxy.isOverlineVal = true;

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
      contentBrowserProxy.htmlTagMap = {[parentId]: 'p'};
      contentBrowserProxy.childrenMap = {[parentId]: [childId]};
      contentBrowserProxy.textContentMap = {[childId]: text};
      contentBrowserProxy.rootId = parentId;

      const root = contentController.updateContent();

      assertTrue(!!root);
      assertEquals('P', root.nodeName);
      assertTrue(!!root.firstChild);
      assertEquals(text, root.textContent);
    });

    test('builds a link as an <a> tag when links are shown', () => {
      const childId = 65;
      const url = 'https://www.google.com/';
      visualBrowserProxy.linksEnabled = true;
      contentBrowserProxy
          .htmlTagMap = {[contentBrowserProxy.rootId]: 'a', [childId]: ''};
      contentBrowserProxy.urlMap = {[contentBrowserProxy.rootId]: url};
      contentBrowserProxy.textContentMap = {[childId]: url};
      contentBrowserProxy
          .childrenMap = {[contentBrowserProxy.rootId]: [childId]};

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLAnchorElement, 'instance');
      assertEquals(url, root.href);
      root.click();
      assertEquals(1, contentBrowserProxy.getCallCount('onLinkClicked'));
    });

    test('builds a link as a <span> tag when links are hidden', () => {
      const childId = 71;
      const url = 'https://www.relsilicon.com/';
      visualBrowserProxy.linksEnabled = false;
      contentBrowserProxy
          .htmlTagMap = {[contentBrowserProxy.rootId]: 'a', [childId]: ''};
      contentBrowserProxy.urlMap = {[contentBrowserProxy.rootId]: url};
      contentBrowserProxy.textContentMap = {[childId]: url};
      contentBrowserProxy
          .childrenMap = {[contentBrowserProxy.rootId]: [childId]};

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
      contentBrowserProxy.rootId = rootId;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'input', [childId]: ''};
      contentBrowserProxy.textContentMap = {[childId]: inputText};
      contentBrowserProxy.childrenMap = {[rootId]: [childId]};
      const root = contentController.updateContent();
      assertTrue(root instanceof HTMLDivElement);
      assertEquals(inputText, root.textContent);
    });

    test('link visibility toggled toggles links with Readability', async () => {
      const url = 'https://www.relsilicon.com/';
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentController.configureTrustedTypes();
      const text = 'a link';
      contentBrowserProxy.htmlContent = `<a href="${url}">${text}</a>`;

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
      visualBrowserProxy.linksEnabled = false;
      contentController.updateLinks(shadowRoot);
      let link = shadowRoot.querySelector('a');
      assertFalse(!!link);
      let span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
      assertTrue(!!span);
      assertEquals(url, span.dataset['link']);
      assertEquals(text, span.textContent);

      // Show the links.
      visualBrowserProxy.linksEnabled = true;
      contentController.updateLinks(shadowRoot);
      span = shadowRoot.querySelector<HTMLElement>('span[data-link]');
      assertFalse(!!span);
      link = shadowRoot.querySelector('a');
      assertTrue(!!link);
      assertEquals(url, link.href);
      assertEquals(text, link.textContent);
    });

    test('builds an image as a <canvas> tag', () => {
      const rootId = contentBrowserProxy.rootId;
      const altText = 'how it\'s done done done';
      visualBrowserProxy.imagesEnabled = true;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'img'};
      contentBrowserProxy.altText = altText;

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLCanvasElement);
      assertEquals(altText, root.getAttribute('alt'));
      assertEquals('', root.style.display);
      assertArrayEquals([rootId], visualBrowserProxy.fetchedImages);
    });

    test('builds a video as a <canvas> tag', () => {
      const rootId = contentBrowserProxy.rootId;
      const altText = 'Huntrx';
      visualBrowserProxy.imagesEnabled = true;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'video'};
      contentBrowserProxy.altText = altText;

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLCanvasElement);
      assertEquals(altText, root.getAttribute('alt'));
      assertEquals('', root.style.display);
      assertArrayEquals([rootId], visualBrowserProxy.fetchedImages);
    });

    test('builds a button as a <div> tag', () => {
      const rootId = 5;
      const childId = 7;
      const buttonText = 'Automatic';

      // Set up the AX Tree with a button that has a text child.
      contentBrowserProxy.rootId = rootId;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'button', [childId]: ''};
      contentBrowserProxy.textContentMap = {[childId]: buttonText};
      contentBrowserProxy.childrenMap = {[rootId]: [childId]};
      const root = contentController.updateContent();
      assertTrue(root instanceof HTMLDivElement);
      assertEquals(buttonText, root.textContent);
    });

    test(
        'builds a button as a <div> tag when Readability enabled', async () => {
          contentBrowserProxy.readabilityEnabled = true;
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          const buttonText = 'Buttons should be seen and not clicked';
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent = `<button>${buttonText}</button>`;

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
          contentBrowserProxy.readabilityEnabled = true;
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          const markText = 'When everything is important, nothing is';
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent = `<mark>${markText}</mark>`;

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
      const rootId = contentBrowserProxy.rootId;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'p', [childId]: ''};
      contentBrowserProxy.textDirection = 'rtl';
      contentBrowserProxy.textContentMap = {[childId]: 'spittin facts'};
      contentBrowserProxy.childrenMap = {[rootId]: [childId]};

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('rtl', root.getAttribute('dir'));
    });

    test('sets the language', () => {
      const childId = 70;
      const rootId = contentBrowserProxy.rootId;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'p', [childId]: ''};
      contentBrowserProxy.language = 'ko';
      contentBrowserProxy.textContentMap = {[childId]: 'you know that\'s'};
      contentBrowserProxy.childrenMap = {[rootId]: [childId]};

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLParagraphElement);
      assertEquals('ko', root.getAttribute('lang'));
    });

    test('builds details as a div', () => {
      const childId = 67;
      const rootId = contentBrowserProxy.rootId;
      contentBrowserProxy.htmlTagMap = {[rootId]: 'details', [childId]: ''};
      contentBrowserProxy.childrenMap = {[rootId]: [childId]};
      contentBrowserProxy.textContentMap = {[childId]: 'I don\'t talk'};

      const root = contentController.updateContent();

      assertTrue(root instanceof HTMLDivElement);
    });

    test(
        'honors links disabled preference on first open with Readability',
        async () => {
          const url = 'https://www.google.com/';
          const text = 'best link ever';
          contentBrowserProxy.readabilityEnabled = true;
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent = `<a href="${url}">${text}</a>`;
          visualBrowserProxy.linksEnabled = false;

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
        'honors links enabled preference on first open with Readability',
        async () => {
          const url = 'https://www.google.com/';
          const text = 'best link ever';
          contentBrowserProxy.readabilityEnabled = true;
          contentBrowserProxy.activeDistillationMethod =
              contentBrowserProxy.distillationTypeReadability;
          contentController.configureTrustedTypes();
          contentBrowserProxy.htmlContent = `<a href="${url}">${text}</a>`;
          visualBrowserProxy.linksEnabled = true;

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
      contentBrowserProxy.htmlTagMap = {[linkId]: 'a'};
      contentBrowserProxy.urlMap = {[linkId]: linkUrl};
    });

    test('does nothing if no content', () => {
      visualBrowserProxy.linksEnabled = false;
      contentController.setState(ContentType.NO_CONTENT);
      contentController.updateLinks(shadowRoot);
      assertFalse(!!shadowRoot.firstChild);
    });

    test('replaces <a> with <span> when hiding links', () => {
      visualBrowserProxy.linksEnabled = false;
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
      visualBrowserProxy.linksEnabled = true;

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
      visualBrowserProxy.linksEnabled = false;

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
      visualBrowserProxy.linksEnabled = true;

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
          visualBrowserProxy.linksEnabled = false;

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
          visualBrowserProxy.linksEnabled = true;

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
          visualBrowserProxy.linksEnabled = false;

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
    test('fetches images', () => {
      const imageId = 33;
      nodeStore.addImageToFetch(imageId);

      contentController.loadImages();

      assertArrayEquals([imageId], visualBrowserProxy.fetchedImages);
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
      contentBrowserProxy.imageBitmap = imageData;
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
      visualBrowserProxy.imagesEnabled = false;
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
      visualBrowserProxy.imagesEnabled = true;
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
      visualBrowserProxy.imagesEnabled = false;
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentController.setState(ContentType.HAS_CONTENT);
      receivedContentChange = false;

      contentController.updateImages(shadowRoot);
      await microtasksFinished();

      assertTrue(receivedContentChange);
    });

    test('updates read aloud state when images are updated', async () => {
      const containerElement = document.createElement('div');
      containerElement.id = 'container';
      shadowRoot.appendChild(containerElement);

      const imageElement = document.createElement('img');
      const captionElement = document.createElement('figcaption');
      captionElement.textContent = 'Image caption';
      figure.appendChild(imageElement);
      figure.appendChild(captionElement);
      containerElement.appendChild(figure);

      visualBrowserProxy.imagesEnabled = true;
      contentController.setState(ContentType.HAS_CONTENT);

      let savedReadAloudState = false;
      let resetForNewContent = false;
      let updateReadAloudStateCalled = false;
      let containerPassedToUpdateReadAloudState = false;

      speechController.hasSpeechBeenTriggered = () => true;
      speechController.saveReadAloudState = () => {
        savedReadAloudState = true;
      };
      speechController.resetForNewContent = () => {
        resetForNewContent = true;
      };
      contentController.updateReadAloudState = (node: Node) => {
        updateReadAloudStateCalled = true;
        containerPassedToUpdateReadAloudState = (node === containerElement);
      };

      contentController.updateImages(shadowRoot);
      await microtasksFinished();

      assertTrue(savedReadAloudState);
      assertTrue(resetForNewContent);
      assertTrue(updateReadAloudStateCalled);
      assertTrue(containerPassedToUpdateReadAloudState);
    });
  });

  suite('onRenderedTextBlocksAvailable', () => {
    setup(() => {
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;
    });

    test('extracts text blocks from container', () => {
      const container = document.createElement('div');
      const text1 = 'Hello ';
      const text2 = 'world';
      container.appendChild(document.createTextNode(text1));
      const span = document.createElement('span');
      span.appendChild(document.createTextNode(text2));
      container.appendChild(span);
      document.body.appendChild(container);

      contentController.onRenderedTextBlocksAvailable(container);

      assertEquals(
          1, contentBrowserProxy.getCallCount('onRenderedTextBlocksAvailable'));
      const sentBlocks = contentBrowserProxy.getArgs(
                             'onRenderedTextBlocksAvailable')[0] as string[];
      assertEquals(2, sentBlocks.length);
      assertEquals(text1, sentBlocks[0]);
      assertEquals(text2, sentBlocks[1]);
    });

    test('does nothing for screen2x', () => {
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeScreen2x;
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = false;
      const container = document.createElement('div');
      container.appendChild(document.createTextNode('Hello'));
      document.body.appendChild(container);

      contentController.onRenderedTextBlocksAvailable(container);

      assertEquals(
          0, contentBrowserProxy.getCallCount('onRenderedTextBlocksAvailable'));
    });

    test('overwrites stored nodes on subsequent calls', () => {
      const container1 = document.createElement('div');
      container1.textContent = 'First call';
      const container2 = document.createElement('div');
      container2.textContent = 'Second call';

      // First call
      contentController.onRenderedTextBlocksAvailable(container1);
      assertEquals(
          1, contentBrowserProxy.getCallCount('onRenderedTextBlocksAvailable'));
      let sentBlocks = contentBrowserProxy.getArgs(
                           'onRenderedTextBlocksAvailable')[0] as string[];
      assertEquals(1, sentBlocks.length);
      assertEquals('First call', sentBlocks[0]);

      // Second call - should replace the internal array
      contentController.onRenderedTextBlocksAvailable(container2);
      assertEquals(
          2, contentBrowserProxy.getCallCount('onRenderedTextBlocksAvailable'));
      sentBlocks = contentBrowserProxy.getArgs(
                       'onRenderedTextBlocksAvailable')[1] as string[];
      assertEquals(1, sentBlocks.length);
      assertEquals('Second call', sentBlocks[0]);
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

      contentBrowserProxy.readabilityEnabled = true;
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.axTreeAnchorsVal = {};
      contentController.setState(ContentType.HAS_CONTENT);
    });

    test('associates dom node when 1:1 match exists', () => {
      contentBrowserProxy
          .axTreeAnchorsVal = {[url]: [{axId: axId, name: 'text'}]};
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor, nodeStore.getDomNode(axId));
    });

    test('resolves ambiguity using HTML ID match (highest priority)', () => {
      anchor.id = 'correct-id';
      contentBrowserProxy.axTreeAnchorsVal = {
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
      contentBrowserProxy.axTreeAnchorsVal = {
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

      contentBrowserProxy.axTreeAnchorsVal = {
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
      contentBrowserProxy.axTreeAnchorsVal = {
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

      contentBrowserProxy.axTreeAnchorsVal = {
        [url]:
            [{axId: 100, name: 'First Link'}, {axId: 200, name: 'Second Link'}],
      };
      contentController.updateAnchorsForReadability(container);

      assertEquals(anchor1, nodeStore.getDomNode(100));
      assertEquals(anchor2, nodeStore.getDomNode(200));
    });

    test('does not associate node when no match exists in map', () => {
      contentBrowserProxy
          .axTreeAnchorsVal = {['https://other.com/']: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test('converts anchor to span when no matching URL is found', () => {
      contentBrowserProxy.axTreeAnchorsVal = {};
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
          contentBrowserProxy.axTreeAnchorsVal = {
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
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeScreen2x;
      contentBrowserProxy.axTreeAnchorsVal = {[url]: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test('does nothing if Readability is not enabled', () => {
      contentBrowserProxy.readabilityEnabled = false;
      contentBrowserProxy.axTreeAnchorsVal = {[url]: [{axId: axId}]};
      contentController.updateAnchorsForReadability(container);

      assertFalse(!!nodeStore.getDomNode(axId));
    });

    test('logs link matches', () => {
      container.replaceChildren();
      const anchor1 = document.createElement('a');
      anchor1.href = url;
      anchor1.textContent = 'First Link';
      container.appendChild(anchor1);

      const anchor2 = document.createElement('a');
      anchor2.href = url;
      anchor2.textContent = 'Second Link';
      container.appendChild(anchor2);

      contentBrowserProxy.axTreeAnchorsVal = {
        [url]:
            [{axId: 100, name: 'First Link'}, {axId: 200, name: 'Second Link'}],
      };

      contentController.updateAnchorsForReadability(container);

      // 4 status calls.
      assertEquals(4, metrics.getCallCount('recordCount'));
    });
  });

  suite('hidden images empty state', () => {
    setup(() => {
      contentBrowserProxy.rootId = 1;
      contentBrowserProxy.htmlTagMap = {1: 'img'};
      contentBrowserProxy.childrenMap = {1: []};
      contentBrowserProxy.textContentMap = {1: ''};
      contentBrowserProxy.hasValidSelectionVal = false;
    });

    test('Images enabled, images available, no text -> empty state', () => {
      visualBrowserProxy.imagesEnabled = true;
      contentController.updateContent();
      assertTrue(contentController.isEmpty());
    });

    test('Images disabled with images and no text -> empty state', () => {
      visualBrowserProxy.imagesEnabled = false;
      contentController.updateContent();
      assertTrue(contentController.isEmpty());
    });

    test(
        'Images enabled, with images and no text, with selection -> has content',
        () => {
          visualBrowserProxy.imagesEnabled = true;
          contentBrowserProxy.hasValidSelectionVal = true;
          const root = contentController.updateContent();
          assertFalse(contentController.isEmpty());
          assertTrue(contentController.hasContent());
          assertTrue(root instanceof HTMLCanvasElement);
        });

    test(
        'Images disabled, with images and no text, with selection -> empty state',
        () => {
          visualBrowserProxy.imagesEnabled = false;
          contentBrowserProxy.hasValidSelectionVal = true;
          contentController.updateContent();
          assertTrue(contentController.isEmpty());
        });
  });

  suite('onRenderedTextMappingReady', () => {
    let container: HTMLElement;
    const axId1 = 101;
    const axId2 = 102;

    setup(() => {
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;

      container = document.createElement('div');
      document.body.appendChild(container);
    });

    test('splits and maps a single node to multiple AX nodes', () => {
      const text = 'Part1Part2Part3';
      const textNode = document.createTextNode(text);
      container.appendChild(textNode);

      contentController.onRenderedTextBlocksAvailable(container);

      contentBrowserProxy.axMapping = [
        {axNodeId: axId1, start: 0, end: 5, axNodeOffset: 0},
        {axNodeId: axId2, start: 5, end: 10, axNodeOffset: 100},
      ];

      contentController.onRenderedTextMappingReady();

      assertEquals(3, container.childNodes.length);
      const node1 = container.childNodes[0]!;
      const node2 = container.childNodes[1]!;
      const node3 = container.childNodes[2]!;

      assertEquals(0, nodeStore.getAxNodeOffset(node1));
      assertEquals(100, nodeStore.getAxNodeOffset(node2));

      assertEquals('Part1', node1.textContent);
      assertEquals('Part2', node2.textContent);
      assertEquals('Part3', node3.textContent);

      assertEquals(axId1, nodeStore.getAxId(node1));
      assertEquals(axId2, nodeStore.getAxId(node2));
      assertFalse(!!nodeStore.getAxId(node3));
    });

    test('handles gaps at the beginning of a block', () => {
      const text = 'GapMapped';
      const textNode = document.createTextNode(text);
      container.appendChild(textNode);
      contentController.onRenderedTextBlocksAvailable(container);

      contentBrowserProxy.axMapping =
          [{axNodeId: axId1, start: 3, end: 9, axNodeOffset: 17}];

      contentController.onRenderedTextMappingReady();

      const mappedNode = container.childNodes[1]!;
      assertEquals(2, container.childNodes.length);
      assertEquals('Gap', container.childNodes[0]!.textContent);
      assertEquals('Mapped', mappedNode.textContent);
      assertEquals(axId1, nodeStore.getAxId(mappedNode));
      assertEquals(17, nodeStore.getAxNodeOffset(mappedNode));
    });

    test('maps multiple segments with different axNodeOffsets', () => {
      const text = 'Segment1Segment2';
      const textNode = document.createTextNode(text);
      container.appendChild(textNode);
      contentController.onRenderedTextBlocksAvailable(container);

      contentBrowserProxy.axMapping = [
        {axNodeId: axId1, start: 0, end: 8, axNodeOffset: 17},
        {axNodeId: axId2, start: 8, end: 16, axNodeOffset: 0},
      ];

      contentController.onRenderedTextMappingReady();

      assertEquals(2, container.childNodes.length);
      const node1 = container.childNodes[0]!;
      const node2 = container.childNodes[1]!;

      assertEquals('Segment1', node1.textContent);
      assertEquals(17, nodeStore.getAxNodeOffset(node1));

      assertEquals('Segment2', node2.textContent);
      assertEquals(0, nodeStore.getAxNodeOffset(node2));
    });

    test('maps text nodes to AX nodes', () => {
      const text = 'Part1Part2Part3';
      const textNode = document.createTextNode(text);
      container.appendChild(textNode);
      contentController.onRenderedTextBlocksAvailable(container);
      contentBrowserProxy.axMapping = [
        {axNodeId: axId1, start: 0, end: 5, axNodeOffset: 0},
      ];

      contentController.onRenderedTextMappingReady();

      const node1 = container.childNodes[0]!;
      assertEquals(axId1, nodeStore.getAxId(node1));
    });

    test('does nothing if feature is disabled', () => {
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = false;
      container.textContent = 'text';
      contentController.onRenderedTextBlocksAvailable(container);

      contentBrowserProxy.axMapping = [];

      contentController.onRenderedTextMappingReady();
      assertEquals(0, contentBrowserProxy.getCallCount('getAxMapping'));
    });
  });

  test('showEmpty event triggers setEmpty', () => {
    contentController.setState(ContentType.HAS_CONTENT);
    contentBrowserProxy.showEmpty.callListeners();
    assertTrue(contentController.isEmpty());
  });

  test('onNodeWillBeDeleted event calls onNodeWillBeDeleted', () => {
    const nodeId = 42;
    const element = document.createElement('div');
    nodeStore.setDomNode(element, nodeId);
    assertTrue(!!nodeStore.getDomNode(nodeId));

    contentBrowserProxy.onNodeWillBeDeleted.callListeners(nodeId);

    assertFalse(!!nodeStore.getDomNode(nodeId));
  });

  test('onImageDownloaded event calls onImageDownloaded', () => {
    const nodeId = 5;
    const canvas = document.createElement('canvas');
    nodeStore.setDomNode(canvas, nodeId);
    contentBrowserProxy.imageBitmap = {
      data: new Uint8ClampedArray([0, 0, 0, 255]),
      width: 1,
      height: 1,
      scale: 1,
    };

    contentBrowserProxy.onImageDownloaded.callListeners(nodeId);

    assertTrue(!!nodeStore.getDomNode(nodeId));
  });
});
